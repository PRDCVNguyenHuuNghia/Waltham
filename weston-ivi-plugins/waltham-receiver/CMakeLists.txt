cmake_minimum_required( VERSION 2.8.5 )

project (waltham-receiver)

find_package(PkgConfig)
find_package (Threads)
pkg_check_modules(WAYLAND_CLIENT wayland-client REQUIRED)
pkg_check_modules(WAYLAND_CURSOR wayland-cursor REQUIRED)
pkg_check_modules(WALTHAM waltham REQUIRED)
pkg_check_modules(GLESv2 glesv2 REQUIRED)
pkg_check_modules(WAYLAND_EGL wayland-egl REQUIRED)
pkg_search_module(EGL egl required)
pkg_search_module(GSTREAMER gstreamer-1.0 required)
pkg_search_module(GSTREAMERAPP gstreamer-app-1.0 required)
pkg_search_module(DRM libdrm required)
pkg_check_modules(IVI-APPLICATION ivi-application REQUIRED)

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
find_library(GST_ALLOCATOR NAMES gstallocators-1.0 PATHs /usr/lib64)
find_library(GST_VIDEO NAMES gstvideo-1.0 PATHs /usr/lib64)
find_library(GSTREAMER_WAYLANDSINK NAMES gstwayland-1.0 PATHs ${LIBS})

include_directories(
    ${WAYLAND_CLIENT_INCLUDE_DIR}
    ${WAYLAND_CURSOR_INCLUDE_DIR}
    ${WALTHAM_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMERAPP_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIR}
    ${GLESv2_INCLUDE_DIRS}
    ${IVI-APPLICATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/waltham-receiver/include/
    ${CMAKE_SOURCE_DIR}/waltham-transmitter/waltham-renderer
)

link_directories(
    ${WAYLAND_CLIENT_LIBRARY_DIRS}
    ${WAYLAND_CURSOR_LIBRARY_DIRS}
    ${WALTHAM_LIBRARY_DIRS}
    ${EGL_LIBRARY_DIRS}
    ${WAYLAND_EGL_LIBRAY_DIRS}
    ${GLESv2_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMERAPP_LIBRARY_DIRS}
    ${IVI-APPLICATION_LIBRARY_DIRS}
)

SET(LIBS
    ${CMAKE_THREAD_LIBS_INIT}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_CURSOR_LIBRARIES}
    ${WALTHAM_LIBRARIES}
    ${EGL_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${GLESv2_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMERAPP_LIBRARIES}
    ${GST_ALLOCATOR}
    ${GST_VIDEO}
    ${IVI-APPLICATION_LIBRARIES}
    ${GSTREAMER_WAYLANDSINK}
)

SET(SRC_FILES
    src/wth-receiver-main.c
    src/wth-receiver-comm.c
    src/wth-receiver-gst.c
    src/utils/bitmap.c
    src/utils/os-compatibility.c
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

add_definitions(${EGL_CFLAGS})

target_link_libraries(${PROJECT_NAME} ${LIBS})

install (TARGETS ${PROJECT_NAME} DESTINATION bin)
