#pragma once

#include <functional>
#include <future>
#include <string>

#include <core/module_dependencies.h>

namespace caspar { namespace qtwebengine {

bool intercept_command_line(int argc, char** argv);
void init(core::module_dependencies dependencies);
void uninit();

}} // namespace caspar::qtquick
