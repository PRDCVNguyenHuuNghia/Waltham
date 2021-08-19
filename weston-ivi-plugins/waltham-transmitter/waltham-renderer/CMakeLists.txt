project (waltham-renderer)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER wayland-server>=1.13.0 REQUIRED)
pkg_check_modules(WESTON weston>=2.0.0 REQUIRED)
pkg_check_modules(PIXMAN pixman-1 REQUIRED)
pkg_check_modules(WALTHAM waltham REQUIRED)
pkg_search_module(GSTREAMER gstreamer-1.0 required)
pkg_search_module(GSTREAMERAPP gstreamer-app-1.0 required)

include_directories(
    ${CMAKE_SOURCE_DIR}/waltham-transmitter/transmitter-plugin
    ${CMAKE_CURRENT_BINARY_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${WESTON_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${WALTHAM_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMERAPP_INCLUDE_DIRS}
)

link_directories(
    ${WAYLAND_SERVER_LIBRARY_DIRS}
    ${WESTON_LIBRARY_DIRS}
    ${PIXMAN_LIBRARY_DIRS}
    ${WALTHAM_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMERAPP_LIBRARY_DIRS}
)

add_library(${PROJECT_NAME} MODULE
        waltham-renderer.c
        waltham-renderer.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

set(LIBS
    m
    weston-6
    gstallocators-1.0
    gstvideo-1.0
    ${WAYLAND_SERVER_LIBRARIES}
    ${WESTON_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    ${WALTHAM_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMERAPP_LIBRARIES}
)

add_dependencies(${PROJECT_NAME} ${LIBS})

target_link_libraries(${PROJECT_NAME} ${LIBS})

install (
    TARGETS             ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/libweston-6
)
