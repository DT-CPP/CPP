project(basic_viewport_ui VERSION 0.1.0 LANGUAGES CXX)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/AnnotationsTool.1/ DESTINATION share/xstudio/plugin/qml/AnnotationsTool.1)

add_custom_target(COPY_ANNO_QML ALL)

add_custom_command(TARGET COPY_ANNO_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/AnnotationsTool.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/AnnotationsTool.1)

