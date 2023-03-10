project(xstudio VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(SOURCES
	xstudio.cpp
	../../../../ui/qml/xstudio/qml.qrc
)


find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Qt5 COMPONENTS Core Quick Gui Widgets OpenGL REQUIRED)
find_package(OpenSSL)
find_package(ZLIB)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME} ${SOURCES})

configure_file(.clang-tidy .clang-tidy)
configure_file(xstudio.sh.in xstudio.sh)

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		xstudio::caf_utility
		xstudio::global
		xstudio::ui::opengl::viewport
		xstudio::ui::qml::bookmark
		xstudio::ui::qml::contact_sheet
		xstudio::ui::qml::embedded_python
		xstudio::ui::qml::event
		xstudio::ui::qml::global_store
		xstudio::ui::qml::helper
		xstudio::ui::qml::media
		xstudio::ui::qml::module
		xstudio::ui::qml::log
		xstudio::ui::qml::timeline
		xstudio::ui::qml::playhead
		xstudio::ui::qml::playlist
		xstudio::ui::qml::quickfuture
		xstudio::ui::qml::session
		xstudio::ui::qml::studio
		xstudio::ui::qml::subset
		xstudio::ui::qml::tag
		xstudio::ui::qml::viewport
		xstudio::ui::viewport
		xstudio::utility
	PUBLIC
		caf::core
		$<$<BOOL:${NVIDIA_HACK}>:GLdispatch>
		Qt5::Gui
		Qt5::Quick
		Qt5::Widgets
		OpenSSL::SSL
		ZLIB::ZLIB
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "${PROJECT_NAME}.bin"
	LINK_DEPENDS_NO_SHARED true
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)

install(PROGRAMS
  ${CMAKE_CURRENT_BINARY_DIR}/xstudio.sh
  DESTINATION bin
  RENAME xstudio)

install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/xstudio_desktop_integration.sh
  DESTINATION bin
  RENAME xstudio_desktop_integration)
