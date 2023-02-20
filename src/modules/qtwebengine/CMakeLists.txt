cmake_minimum_required (VERSION 3.10)
project (qtwebengine)

set(SOURCES
		# producer/qtquick_cg_proxy.cpp
		producer/qtwebengine_producer.cpp

		app.cpp
		qtwebengine.cpp
)
set(HEADERS
		# producer/qtquick_cg_proxy.h
		producer/qtwebengine_producer.h

		app.h
		qtwebengine.h
)

add_library(qtwebengine ${SOURCES} ${HEADERS})

set_property(TARGET qtwebengine PROPERTY CXX_STANDARD 17)

include_directories(..)
include_directories(../..)
include_directories(${BOOST_INCLUDE_PATH})
include_directories(${TBB_INCLUDE_PATH})
include_directories(${QT6_INCLUDE_PATH})

#find_package(Qt6 REQUIRED COMPONENTS Core Gui WebEngineWidgets)
#qt6_standard_project_setup()

target_link_libraries(qtwebengine Qt6::Core Qt6::Quick Qt6Widgets Qt6WebEngineCore Qt6WebEngineWidgets)

set_target_properties(qtwebengine PROPERTIES FOLDER modules)
source_group(sources\\producer producer/*)
source_group(sources ./*)

target_link_libraries(qtwebengine
  common
  core
)

casparcg_add_include_statement("modules/qtwebengine/qtwebengine.h")
casparcg_add_init_statement("qtwebengine::init" "qtwebengine")
casparcg_add_uninit_statement("qtwebengine::uninit")
casparcg_add_command_line_arg_interceptor("qtwebengine::intercept_command_line")
casparcg_add_module_project("qtwebengine")