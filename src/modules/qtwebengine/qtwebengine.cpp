#include "qtwebengine.h"
#include "app.h"

//#include "producer/qtquick_cg_proxy.h"
#include "producer/qtwebengine_producer.h"

#include <core/producer/cg_proxy.h>

#include <boost/property_tree/ptree.hpp>

#include <common/log.h>

#include <QThread>
#include <QtGlobal>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>
#include <QtWidgets/QApplication>

#include <memory>

namespace caspar { namespace qtwebengine {

bool intercept_command_line(int argc, char** argv) { return start_qt_application(argc, argv); }

void init(core::module_dependencies dependencies)
{
    dependencies.producer_registry->register_producer_factory(L"QtWebEngine Producer", qtwebengine::create_producer);

    /*
    dependencies.cg_registry->register_cg_producer(
        L"qtquick",
        {L".qml"},
        [](const spl::shared_ptr<core::frame_producer>& producer) {
            return spl::make_shared<qtquick_cg_proxy>(producer);
        },
        [](const core::frame_producer_dependencies& dependencies, const std::wstring& filename) {
            return qtquick::create_producer(dependencies, {filename});
        },
        false);*/
}

void uninit() { stop_qt_application(); }

}} // namespace caspar::qtwebengine
