#include "app.h"

#include <common/log.h>

#include <QThread>
#include <QtGlobal>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>
#include <QtWebEngineQuick/QtWebEngineQuick>
#include <QGuiApplication>

#include <memory>

// workaround to give QGuiApplication a valid reference for argc
// see https://stackoverflow.com/questions/43825082/qcoreapplication-on-the-heap/43825083#43825083
struct ApplicationHolder
{
    ApplicationHolder(int argc, char* argv[])
        : m_argc(new int(argc))
        , m_app(new QGuiApplication(*m_argc, argv))
    {
        // m_argc = new int(argc);
        // m_app  = new QApplication(*m_argc, argv);
    }
    ~ApplicationHolder()
    {
        delete m_app;
        delete m_argc;
    }

    int*          m_argc;
    QGuiApplication* m_app; // TODO - should this be QCoreApplication or QGuiApplication?
};

namespace {
// Use a single Qt application event loop for all producers.
std::unique_ptr<ApplicationHolder> application;
std::thread                        thread;

std::mutex              application_started_mutex;
bool                    application_started = false;
std::condition_variable application_started_condition;
} // namespace

namespace caspar { namespace qtwebengine {

// TODO - this is a hack
QGuiApplication* get_qt_application() { return application->m_app; }

bool start_qt_application(int argc, char** argv)
{
    thread = std::thread([argc, argv]() {

        QCoreApplication::setOrganizationName("CasparCG");
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

        QtWebEngineQuick::initialize();

        application = std::make_unique<ApplicationHolder>(argc, argv);

        // only functional when Qt Quick is also using OpenGL
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

        //QWebEngineProfile::defaultProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
        //QWebEngineProfile::defaultProfile()->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);

        QMetaObject::invokeMethod(
            application->m_app,
            []() {
                application_started = true;
                // TODO - does this need to aquire application_started_condition?
                application_started_condition.notify_all();
            },
            Qt::AutoConnection);
        int exit_code = application->m_app->exec();

        CASPAR_LOG(fatal) << "Qt application loop exited with code " << std::to_string(exit_code);
    });

    {
        // Wait for application event loop to be running.
        std::unique_lock<std::mutex> lock(application_started_mutex);
        application_started_condition.wait(lock, []() { return application_started; });
    }

    return false;
}

void stop_qt_application()
{
    application->m_app->exit(0);
    application.reset();
}

}} // namespace caspar::qtwebengine
