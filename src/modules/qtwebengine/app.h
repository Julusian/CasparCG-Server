#pragma once

#include <QtWidgets/QApplication>

namespace caspar { namespace qtwebengine {

QApplication* get_qt_application();
bool          start_qt_application(int argc, char** argv);
void          stop_qt_application();

}} // namespace caspar::qtwebengine
