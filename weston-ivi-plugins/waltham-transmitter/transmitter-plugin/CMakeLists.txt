project (transmitter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER wayland-server>=1.13.0 REQUIRED)
pkg_check_modules(WESTON weston>=2.0.0 REQUIRED)
pkg_check_modules(PIXMAN pixman-1 REQUIRED)
pkg_check_modules(WALTHAM waltham REQUIRED)

include_directories(
    include
    ${CMAKE_SOURCE_DIR}/waltham-transmitter/waltham-renderer
    ${CMAKE_CURRENT_BINARY_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${WESTON_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${WALTHAM_INCLUDE_DIRS}
)

link_directories(
    ${WAYLAND_SERVER_LIBRARY_DIRS}
    ${WESTON_LIBRARY_DIRS}
    ${PIXMAN_LIBRARY_DIRS}
    ${WALTHAM_LIBRARY_DIRS}
)

add_library(${PROJECT_NAME} MODULE
    plugin.c
    output.c
    input.c
    plugin.h
    transmitter_api.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

set(LIBS
    m
    ${WAYLAND_SERVER_LIBRARIES}
    ${WESTON_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    ${WALTHAM_LIBRARIES}
)

SET(SRC_FILES
    plugin.c
    output.c
    input.c
    plugin.h
    transmitter_api.h
)



add_dependencies(${PROJECT_NAME} ${LIBS})

target_link_libraries(${PROJECT_NAME} ${LIBS})

install (
    TARGETS             ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/weston

)
