#pragma once

#include <QGuiApplication>

namespace caspar { namespace qtwebengine {

QGuiApplication* get_qt_application();
bool          start_qt_application(int argc, char** argv);
void          stop_qt_application();

}} // namespace caspar::qtwebengine
