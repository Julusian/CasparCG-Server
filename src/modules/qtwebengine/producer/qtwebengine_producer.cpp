#include "qtwebengine_producer.h"

#include <core/video_format.h>

#include <core/frame/draw_frame.h>
#include <core/frame/frame.h>
#include <core/frame/frame_factory.h>
#include <core/frame/pixel_format.h>
#include <core/monitor/monitor.h>
#include <core/producer/frame_producer.h>

#include <common/assert.h>
#include <common/diagnostics/graph.h>
#include <common/env.h>
#include <common/future.h>
#include <common/log.h>
#include <common/os/filesystem.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer.hpp>
#include <memory>

#include <tbb/concurrent_queue.h>

#include <GL/glew.h>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineRegisterProtocolHandlerRequest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <atomic>
#include <common/gl/gl_check.h>
#include <utility>

#include "../app.h"

namespace caspar { namespace qtwebengine {

/*
class WebPage : public QWebEnginePage
{
    2Q_OBJECT

  public:
    WebPage(QWebEngineProfile* profile, QObject* parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
        connect(this, &QWebEnginePage::selectClientCertificate, this, &WebPage::handleSelectClientCertificate);
        connect(this, &QWebEnginePage::certificateError, this, &WebPage::handleCertificateError);
    }

  private slots:
    void handleCertificateError(QWebEngineCertificateError error)
    {
        // Always allow all certificates
        error.acceptCertificate();
    }

    void handleSelectClientCertificate(QWebEngineClientCertificateSelection selection)
    {
        // Just select one.
        selection.select(selection.certificates().at(0));
    }
};
 */

class MyFrame
{
  public:
    std::shared_ptr<QOpenGLContext> context_;
    QOffscreenSurface               surface_;

    uint  textureId_ = 0;
    QSize textureSize_;

    explicit MyFrame(std::shared_ptr<QOpenGLContext> context)
        : context_(std::move(context))
    {
        // Pass m_context->format(), not format. Format does not specify and color buffer
        // sizes, while the context, that has just been created, reports a format that has
        // these values filled in. Pass this to the offscreen surface to make sure it will be
        // compatible with the context's configuration.
        surface_.setFormat(context_->format());
        surface_.create();
    }

    ~MyFrame()
    {
        // Make sure the context is current while doing cleanup.
        context_->makeCurrent(&surface_);

        destroyTexture();

        //  surface_.destroy();
        context_->doneCurrent();
    }

    /*
    void resizeTexture(QSize size)
    {
        if (context_->makeCurrent(&surface_)) {
            context_->functions()->glDeleteTextures(1, &textureId_);
            textureId_ = 0;
            createTexture(size);
            context_->doneCurrent();
        }
    }
    */

    void createTexture(QSize size)
    {
        // The scene graph has been initialized. It is now time to create an texture and associate
        // it with the QQuickWindow.
        // m_dpr = devicePixelRatio();
        textureSize_        = size;
        QOpenGLFunctions* f = context_->functions();
        f->glGenTextures(1, &textureId_);
        f->glBindTexture(GL_TEXTURE_2D, textureId_);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        f->glTexImage2D(GL_TEXTURE_2D,
                        0,
                        GL_RGBA,
                        textureSize_.width(),
                        textureSize_.height(),
                        0,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        nullptr);
    }

    void destroyTexture()
    {
        context_->functions()->glDeleteTextures(1, &textureId_);
        textureId_ = 0;
    }

    void bind(QQuickWindow* window)
    {
        CASPAR_LOG(info) << "bind " << textureId_ << " " << textureSize_.width() << "x" << textureSize_.height();
        window->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(textureId_, textureSize_));
    }
};

class qtwebengine_view
{
    using loaded_callback_t = std::function<void()>;

    const loaded_callback_t                    loaded_callback_;
    const core::video_format_desc              format_desc_;
    const spl::shared_ptr<core::frame_factory> frame_factory_;

    QQuickRenderControl             control_;
    std::shared_ptr<QOpenGLContext> context_;
    QQmlEngine                      engine_;
    QQuickWindow                    window_;
    std::unique_ptr<QQmlComponent>  qmlComponent_;

    MyFrame* my_frame_;

    spl::shared_ptr<diagnostics::graph> graph_;

    QTimer                      updateTimer_;
    std::unique_ptr<QQuickItem> rootItem_;

    std::unique_ptr<QWebEnginePage> web_page_;
    // std::unique_ptr<QWebEngineView> web_view_;

    std::unique_ptr<QOpenGLFramebufferObject> fbo_;

    mutable std::mutex frame_mutex_;
    core::draw_frame   frame_;

    void createFbo()
    {
        if (!context_->makeCurrent(&my_frame_->surface_))
            return;

        my_frame_->createTexture(QSize(format_desc_.width, format_desc_.height));
        //         auto v = QQuickRenderTarget::fromOpenGLTexture();

        fbo_ = std::make_unique<QOpenGLFramebufferObject>(QSize(format_desc_.width, format_desc_.height),
                                                          QOpenGLFramebufferObject::CombinedDepthStencil,
                                                          GL_TEXTURE_2D,
                                                          GL_RGBA);

        // window_.setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(fbo_->texture(), fbo_->size()));
        // window_.setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(my_frame_->textureId_,
        // my_frame_->textureSize_));
    }

    void destroyFbo()
    {
        if (!context_->makeCurrent(&my_frame_->surface_))
            return;

        my_frame_->destroyTexture();
        fbo_.reset(nullptr);
    }

    void requestUpdate()
    {
        // Frame has been consumer, trigger another draw
        // TODO - this is going to give such inconsistent fps..
        if (!updateTimer_.isActive())
            updateTimer_.start();
    }

    void render()
    {
        CASPAR_LOG(debug) << "rrender " << my_frame_->textureId_ << " " << QThread::currentThread();

        boost::timer timer;
        timer.restart();

        if (!context_->makeCurrent(&my_frame_->surface_))
            return;

        if (my_frame_->textureId_ == 0) {
            // Ensure frame has been created
           // my_frame_->createTexture(window_.size());
        }

        // TODO - this is needed to bind frame2 to be drawn to (in theory)
        // my_frame_->bind(&window_);
        window_.setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(fbo_->texture(), fbo_->size()));

        // Polish, synchronize and render the next frame (into our texture).  In this example
        // everything happens on the same thread and therefore all three steps are performed
        // in succession from here. In a threaded setup the render() call would happen on a
        // separate thread.
        control_.beginFrame();
        control_.polishItems();
        control_.sync();
        control_.render();
        control_.endFrame();

        // window_.resetOpenGLState();
        //QOpenGLFramebufferObject::bindDefault();

        QOpenGLFunctions* f = context_->functions();

        auto fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        f->glFlush();

        // No errors, but using this frame gives only black

        while (fence != nullptr) {
            auto wait = glClientWaitSync(fence, 0, 0);
            if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED) {
                glDeleteSync(fence);
                fence = nullptr;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        glDeleteSync(fence);

        if (!fbo_) {
            CASPAR_LOG(debug) << "no fbo";
            return;
        }

        CASPAR_LOG(debug) << "render fbo" << fbo_->texture();

        QImage image  = fbo_->toImage();
        int    width  = image.width();
        int    height = image.height();

        core::pixel_format_desc pixel_desc;
        pixel_desc.format = core::pixel_format::bgra;
        pixel_desc.planes.emplace_back(width, height, 4);

        auto frame2 = frame_factory_->import_gl_texture(this, fbo_->texture(), fbo_->width(), fbo_->height());

        auto                 frame  = frame_factory_->create_frame(this, pixel_desc);
        const unsigned char* buffer = image.bits();
        // std::memcpy(frame.image_data(0).begin(), buffer, width * height * 4);

        GL(f->glBindTexture(GL_TEXTURE_2D, fbo_->texture()));

        GL(f->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame.image_data(0).begin()));

        GL(f->glBindTexture(GL_TEXTURE_2D, 0));

        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            frame_ = core::draw_frame(std::move(frame));
        }

        graph_->set_value("render-time", timer.elapsed() * format_desc_.fps);
    }

    void run()
    {
        if (qmlComponent_->isError()) {
            const QList<QQmlError> errorList = qmlComponent_->errors();
            for (const QQmlError& error : errorList)
                qWarning() << error.url() << error.line() << error;
            return;
        }

        QObject* rootObject = qmlComponent_->create();
        if (qmlComponent_->isError()) {
            const QList<QQmlError> errorList = qmlComponent_->errors();
            for (const QQmlError& error : errorList)
                qWarning() << error.url() << error.line() << error;
            return;
        }

        rootItem_.reset(qobject_cast<QQuickItem*>(rootObject));
        if (!rootItem_) {
            qWarning("run: Not a QQuickItem");
            delete rootObject;
            return;
        }

        // The root item is ready. Associate it with the window.
        rootItem_->setParentItem(window_.contentItem());

        // Update item and rendering related geometries.
        updateSizes();

        // Initialize the render control and our OpenGL resources.
        context_->makeCurrent(&my_frame_->surface_);
        window_.setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(context_.get()));
        control_.initialize();

        QMetaObject::invokeMethod(get_qt_application(), [this]() { requestUpdate(); });
        loaded_callback_();
    }

    void updateSizes()
    {
        // Behave like SizeRootObjectToView.
        rootItem_->setWidth(format_desc_.width);
        rootItem_->setHeight(format_desc_.height);

        window_.setGeometry(0, 0, format_desc_.width, format_desc_.height);
    }

  public:
    qtwebengine_view(const spl::shared_ptr<core::frame_factory>& frame_factory,
                     const loaded_callback_t&                    loaded_callback,
                     core::video_format_desc                     format_desc)
        : loaded_callback_(loaded_callback)
        , format_desc_(std::move(format_desc))
        , frame_factory_(frame_factory)
        //, web_page_(std::make_unique<QWebEnginePage>())
        //, web_view_(std::make_unique<QWebEngineView>())
        , window_(&control_)
        , frame_(core::draw_frame{})
    {
        graph_->set_color("render-time", diagnostics::color(0.1f, 1.0f, 0.1f));
        graph_->set_text(L"test");
        diagnostics::register_graph(graph_);

        // All of Qt must be initialized from Qt's main thread to work.
        // TODO - this check is failing, but everything is working..
        // assert(QThread::currentThread() == get_qt_application()->thread());

        // QObject::connect(web_page_.get(), &QWebEnginePage::featurePermissionRequested, this,
        // &WebView::handleFeaturePermissionRequested);

        /*
        QObject::connect(web_page_.get(),
                         &QWebEnginePage::selectClientCertificate,
                         [this](QWebEngineClientCertificateSelection selection) {
                             // Just select one.
                             selection.select(selection.certificates().at(0));
                         });
        QObject::connect(web_page_.get(), &QWebEnginePage::certificateError, [this](QWebEngineCertificateError error) {
            // Always allow all certificates
            error.acceptCertificate();
        });

        QObject::connect(web_view_.get(), &QWebEngineView::loadStarted, [this]() { CASPAR_LOG(info) << "Loading"; });
        QObject::connect(web_view_.get(), &QWebEngineView::loadProgress, [this](int progress) {});
        QObject::connect(web_view_.get(), &QWebEngineView::loadFinished, [this](bool success) {
            CASPAR_LOG(info) << (success ? "Load successful" : "Load failed");
        });

        QObject::connect(web_view_.get(),
                         &QWebEngineView::renderProcessTerminated,
                         [this](QWebEnginePage::RenderProcessTerminationStatus termStatus, int statusCode) {
                             // TODO - some better error handling
                             switch (termStatus) {
                                 case QWebEnginePage::NormalTerminationStatus:
                                     CASPAR_LOG(warning) << "Render process normal exit";
                                     break;
                                 case QWebEnginePage::AbnormalTerminationStatus:
                                     CASPAR_LOG(warning) << "Render process abnormal exit";
                                     break;
                                 case QWebEnginePage::CrashedTerminationStatus:
                                     CASPAR_LOG(warning) << "Render process crashed";
                                     break;
                                 case QWebEnginePage::KilledTerminationStatus:
                                     CASPAR_LOG(warning) << "Render process killed";
                                     break;
                             }
                         });

        web_view_->setPage(web_page_.get());
        web_view_->setUrl(QUrl(QStringLiteral("https://www.qt.io")));
        */

        // web_view_->render

        // web_view_->show();

        QSurfaceFormat format;
        // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
        format.setDepthBufferSize(16);
        format.setStencilBufferSize(8);

        auto native_context_id = frame_factory_->hack_context_id();
        CASPAR_LOG(info) << L"Have native context id: " << native_context_id;
        CASPAR_ASSERT(native_context != 0);

        auto parent_share_context = QNativeInterface::QGLXContext::fromNative((GLXContext)native_context_id);

        context_ = std::make_shared<QOpenGLContext>();
        context_->setShareContext(parent_share_context);
        context_->setFormat(format);
        context_->create();

        my_frame_ = new MyFrame(context_);

        // window_.setColor(Qt::transparent);
        window_.setColor(QColor(255, 0, 0, 255));

        if (!engine_.incubationController())
            engine_.setIncubationController(window_.incubationController());

        // Trigger the first render() in a few ms
        updateTimer_.setSingleShot(true);
        updateTimer_.setInterval(5);
        QObject::connect(&updateTimer_, &QTimer::timeout, [this]() { render(); });

        QObject::connect(&window_, &QQuickWindow::sceneGraphInitialized, [this]() { createFbo(); });

        QObject::connect(&window_, &QQuickWindow::sceneGraphInvalidated, [this]() { destroyFbo(); });
        QObject::connect(&window_,
                         &QQuickWindow::sceneGraphError,
                         [this](QQuickWindow::SceneGraphError error, const QString& message) {
                             CASPAR_LOG(error) << "Scene graph failed: " << message.toStdString();
                         });
        QObject::connect(
            &window_, &QQuickWindow::sceneGraphAboutToStop, [this]() { CASPAR_LOG(info) << "sceneGraphAboutToStop"; });

        QObject::connect(
            &control_, &QQuickRenderControl::renderRequested, [this]() { CASPAR_LOG(info) << "renderRequested"; });
        QObject::connect(
            &control_, &QQuickRenderControl::sceneChanged, [this]() { CASPAR_LOG(info) << "sceneChanged"; });
    }

    ~qtwebengine_view()
    {
        context_->makeCurrent(&my_frame_->surface_);
        control_.invalidate();
    }

    void startQuick(const QString& filename)
    {
        qmlComponent_ = std::make_unique<QQmlComponent>(&engine_, QUrl(filename));

        if (qmlComponent_->isLoading()) {
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn     = QObject::connect(qmlComponent_.get(), &QQmlComponent::statusChanged, [this, conn]() {
                QObject::disconnect(*conn);
                run();
                });
        } else {
            run();
        }
    }

    void execute_javascript(const std::vector<std::wstring>& params)
    {
        /*
        assert(rootItem_);
        if (params.size() == 1)
            QMetaObject::invokeMethod(rootItem_.get(), u8(params.at(0)).c_str());
        else
            QMetaObject::invokeMethod(
                rootItem_.get(), u8(params.at(0)).c_str(), Q_ARG(QVariant, QString::fromStdWString(params.at(1))));
        */
    }

    core::draw_frame receive_frame()
    {
        core::draw_frame frame;

        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            frame = frame_;
        }

        QMetaObject::invokeMethod(get_qt_application(), [this]() { requestUpdate(); });

        return frame;
    }
};

class qtwebengine_producer : public core::frame_producer
{
    const std::wstring url_;

    std::unique_ptr<qtwebengine_view> renderer_;

    tbb::concurrent_queue<std::vector<std::wstring>> javascript_before_load_;
    std::atomic<bool>                                loaded_;

  public:
    qtwebengine_producer(const spl::shared_ptr<core::frame_factory>& frame_factory,
                         const core::video_format_desc&              format_desc,
                         const std::wstring&                         url)
        : url_(url)
    {
        loaded_ = false;

        QMetaObject::invokeMethod(
            get_qt_application(),
            [this, url, frame_factory, format_desc]() {
                auto loaded_callback = [this]() { renderer_loaded(); };
                renderer_            = std::make_unique<qtwebengine_view>(frame_factory, loaded_callback, format_desc);
                renderer_->startQuick("demo.qml");
            },
            Qt::QueuedConnection);
    }

    ~qtwebengine_producer()
    {
        qtwebengine_view* renderer = renderer_.release();
        QMetaObject::invokeMethod(
            get_qt_application(), [renderer]() { delete renderer; }, Qt::QueuedConnection);
    }

    void renderer_loaded()
    {
        loaded_ = true;
        execute_queued_javascript();
    }

    std::wstring name() const override { return L"qtwebengine"; }

    core::draw_frame receive_impl(const core::video_field field, int nb_samples) override
    {
        // TODO - race condition on renderer_ here and everywhere?
        if (renderer_) {
            return renderer_->receive_frame();
        }

        return core::draw_frame{};
    }

    std::future<std::wstring> call(const std::vector<std::wstring>& params) override
    {
        if (!loaded_) {
            javascript_before_load_.push(params);
        } else {
            execute_queued_javascript();
            renderer_->execute_javascript(params);
        }

        return make_ready_future(std::wstring(L""));
    }

    void execute_queued_javascript()
    {
        std::vector<std::wstring> params;

        while (javascript_before_load_.try_pop(params))
            renderer_->execute_javascript(params);
    }

    std::wstring print() const override { return L"qtwebengine[" + url_ + L"]"; }

    core::monitor::state state() const override
    {
        static const core::monitor::state empty;
        return empty;
    }
};

spl::shared_ptr<core::frame_producer> create_producer(const core::frame_producer_dependencies& dependencies,
                                                      const std::vector<std::wstring>&         params)
{
    const auto html_prefix    = boost::iequals(params.at(0), L"[HTML]");
    const auto param_url      = html_prefix ? params.at(1) : params.at(0);
    const auto filename       = env::template_folder() + param_url + L".html";
    const auto found_filename = find_case_insensitive(filename);
    const auto http_prefix =
        boost::algorithm::istarts_with(param_url, L"http:") || boost::algorithm::istarts_with(param_url, L"https:");

    if (!found_filename && !http_prefix && !html_prefix)
        return core::frame_producer::empty();

    // TODO - is this qt compatible
    const auto url = found_filename ? L"file://" + *found_filename : param_url;

    return core::create_destroy_proxy(
        spl::make_shared<qtwebengine_producer>(dependencies.frame_factory, dependencies.format_desc, url));
}

}} // namespace caspar::qtwebengine
