/*
 * Copyright © 2019 Advanced Driver Information Technology GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*******************************************************************************
**                                                                            **
**  TARGET    : linux                                                         **
**                                                                            **
**  PROJECT   : waltham-receiver                                              **
**                                                                            **
**  PURPOSE   : Header file declare macros, extern functions, data types etc, **
**  required to interface with waltham IPC library                            **
**                                                                            **
*******************************************************************************/

#ifndef WTH_SERVER_WALTHAM_COMM_H_
#define WTH_SERVER_WALTHAM_COMM_H_

#include <stdio.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>

#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-egl.h>
#include <wayland-client.h>
#include <waltham-server.h>
#include <waltham-connection.h>

#define DEBUG 0

struct receiver;
struct client;
struct window;
struct pointer;
struct touch;
static int verbose = 0;

const struct wth_display_interface display_implementation;

/**
* receiver_accept_client
*
* Accepts new waltham client connection and instantiates client structure
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void receiver_accept_client(struct receiver *srv);

/**
* receiver_flush_clients
*
* write all the pending requests from the clients to socket
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void receiver_flush_clients(struct receiver *srv);

/**
* client_destroy
*
* Destroy client connection
*
* @param names        struct client *c
* @param value        client data
* @return             none
*/
void client_destroy(struct client *c);

/**
* waltham_pointer_enter
*
* Send pointer enter event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
*             wl_fixed_t sx
*             wl_fixed_t sy
* @param value        window - window information
*                     serial - serial number of the enter event
*             sx     - surface-local x coordinate
*             sy     - surface-local y coordinate
* @return             none
*/
void waltham_pointer_enter(struct window *window, uint32_t serial,
                      wl_fixed_t sx, wl_fixed_t sy);

/**
* waltham_pointer_leave
*
* Send pointer leave event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
* @param value        window - window information
*                     serial - serial number of the leave event
* @return             none
*/
void waltham_pointer_leave(struct window *window, uint32_t serial);

/**
* waltham_pointer_motion
*
* Send pointer motion event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t time
*             wl_fixed_t sx
*             wl_fixed_t sy
* @param value        window - window information
*                     time   - timestamp with millisecond granularity
*             sx     - surface-local x coordinate
*             sy     - surface-local y coordinate
* @return             none
*/
void waltham_pointer_motion(struct window *window, uint32_t time,
                       wl_fixed_t sx, wl_fixed_t sy);

/**
* waltham_pointer_button
*
* Send pointer button event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
*             uint32_t time
*                     uint32_t button
*             uint32_t state
* @param value        window - window information
*             serial - serial number of the button event
*                     time   - timestamp with millisecond granularity
*             button - button that produced the event
*             state  - physical state of the button
* @return             none
*/
void waltham_pointer_button(struct window *window, uint32_t serial,
               uint32_t time, uint32_t button,
               uint32_t state);

/**
* waltham_pointer_axis
*
* Send pointer axis event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t time
*             uint32_t axis
*             wl_fixed_t value
* @param value        window - window information
*             time   - timestamp with millisecond granularity
*             axis   - axis type
*                     value  - length of vector in surface-local coordinate space
* @return             none
*/
void waltham_pointer_axis(struct window *window, uint32_t time,
             uint32_t axis, wl_fixed_t value);

/**
* waltham_touch_down
*
* Send touch down event received from weston to waltham client
*
* @param names     struct window *window
*          uint32_t serial
*                  uint32_t time
*          int32_t id
*          wl_fixed_t x_w
*          wl_fixed_t y_w
* @param value     window - window information
*          serial - serial number of the touch down event
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
*          x_w    - surface-local x coordinate
*          y_w    - surface-local y coordinate
* @return          none
*/
void waltham_touch_down(struct window *window, uint32_t serial,
                   uint32_t time, int32_t id,
           wl_fixed_t x_w, wl_fixed_t y_w);

/**
* waltham_touch_up
*
* Send touch up event received from weston to waltham client
*
* @param names     struct window *window
*          uint32_t serial
*                  uint32_t time
*          int32_t id
* @param value     window - window information
*          serial - serial number of the touch up event
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
* @return          none
*/
void waltham_touch_up(struct window *window, uint32_t serial,
                 uint32_t time, int32_t id);

/**
* waltham_touch_motion
*
* Send touch motion event received from weston to waltham client
*
* @param names     struct window *window
*                  uint32_t time
*          int32_t id
*          wl_fixed_t x_w
*          wl_fixed_t y_w
* @param value     window - window information
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
*          x_w    - surface-local x coordinate
*          y_w    - surface-local y coordinate
* @return          none
*/
void waltham_touch_motion(struct window *window, uint32_t time,
             int32_t id, wl_fixed_t x_w, wl_fixed_t y_w);

/**
* waltham_touch_frame
*
* Send touch frame event received from weston to waltham client
*
* @param names        struct window *window
* @param value        window - window information
* @return             none
*/
void waltham_touch_frame(struct window *window);

/**
* waltham_touch_cancel
*
* Send touch cancel event received from weston to waltham client
*
* @param names        struct window *window
* @param value        window - window information
* @return             none
*/
void waltham_touch_cancel(struct window *window);

/**
 * set verbosity
 */
inline void set_verbosity(int verbosity)
{
	verbose = verbosity;
}

/**
 * get verbosity
 */
inline bool get_verbosity()
{
    if (verbose == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/***** macros *******/
#define MAX_EPOLL_WATCHES 2

#ifndef container_of
#define container_of(ptr, type, member) ({                              \
        const __typeof__( ((type *)0)->member ) *__mptr = (ptr);        \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define wl_list_last_until_empty(pos, head, member)                     \
        while (!wl_list_empty(head) &&                                  \
                (pos = wl_container_of((head)->prev, pos, member), 1))

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

#ifndef pr_fmt
#   define pr_fmt(fmt) fmt
#endif

#define wth_error(fmt, ...) \
    ({ fprintf(stderr, pr_fmt(fmt), ## __VA_ARGS__); fflush(stderr); })

#if DEBUG
#define wth_verbose(fmt, ...) \
    ({ if (get_verbosity()) { fprintf(stdout, pr_fmt(fmt), ## __VA_ARGS__); fflush(stdout); } })
#else
#define wth_verbose(fmt, ...)\
		({})
#endif

/****** incline functions *****/
static inline void *
zalloc(size_t size)
{
        return calloc(1, size);
}


/***** Data types *****/
/* wthp_region protocol object */
struct region {
    struct wthp_region *obj;
    /* pixman_region32_t region; */
    struct wl_list link; /* struct client::region_list */
};

/* wthp_compositor protocol object */
struct compositor {
    struct wthp_compositor *obj;
    struct client *client;
    struct wl_list link; /* struct client::compositor_list */
};

/* wthp_blob_factory protocol object */
struct blob_factory {
    struct wthp_blob_factory *obj;
    struct client *client;
    struct wl_list link; /* struct client::blob_factory_list */
};

/* wthp_buffer protocol object */
struct buffer {
    struct wthp_buffer *obj;
    uint32_t data_sz;
    void *data;
    int32_t width;
    int32_t height;
    int32_t stride;
    uint32_t format;
    struct wl_list link; /* struct client::buffer_list */
};

/* wthp_seat protocol object */
struct seat {
    struct wthp_seat *obj;
    struct client *client;
    struct pointer *pointer;
    struct keyboard *keyboard;
    struct touch *touch;
    struct wl_list link; /* struct client::seat_list */
};

/* wthp_pointer protocol object */
struct pointer {
    struct wthp_pointer *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::pointer_list */
};

/* wthp_keyboard protocol object */
struct keyboard {
    struct wthp_keyboard *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::keyboard_list */
};

/* wthp_touch protocol object */
struct touch {
    struct wthp_touch *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::touch_list */
};

/* wthp_surface protocol object */
struct surface {
    struct wthp_surface *obj;
    uint32_t ivi_id;
    struct ivisurface *ivisurf;
    struct wthp_callback *cb;
    struct window *shm_window;
    struct wl_list link; /* struct client::surface_list */
};
/* wthp_ivi_surface protocol object */
struct ivisurface {
    struct wthp_ivi_surface *obj;
    struct wthp_callback *cb;
    struct wl_list link; /* struct client::surface_list */
    struct surface *surf;
};

/* wthp_ivi_application protocol object */
struct application {
        struct wthp_ivi_application *obj;
        struct client *client;
        struct wl_list link; /* struct client::surface_list */
};

/* wthp_registry protocol object */
struct registry {
    struct wthp_registry *obj;
    struct client *client;
    struct wl_list link; /* struct client::registry_list */
};

enum wthp_seat_capability {
    /**
     * the seat has pointer devices
     */
    WTHP_SEAT_CAPABILITY_POINTER = 1,
    /**
     * the seat has one or more keyboards
     */
    WTHP_SEAT_CAPABILITY_KEYBOARD = 2,
    /**
     * the seat has touch devices
     */
    WTHP_SEAT_CAPABILITY_TOUCH = 4,
};

/* epoll structure */
struct watch {
    struct receiver *receiver;
    int fd;
    void (*cb)(struct watch *w, uint32_t events);
};

struct client {
    struct wl_list link; /* struct receiver::client_list */
    struct receiver *receiver;

    struct wth_connection *connection;
    struct watch conn_watch;

    /* client object lists for clean-up on disconnection */
    struct wl_list registry_list;     /* struct registry::link */
    struct wl_list compositor_list;   /* struct compositor::link */
    struct wl_list region_list;       /* struct region::link */
    struct wl_list surface_list;      /* struct surface::link */
    struct wl_list buffer_list;       /* struct buffer::link */
    struct wl_list seat_list;         /* struct seat::link */
    struct wl_list pointer_list;      /* struct pointer::link */
    struct wl_list touch_list;        /* struct touch::link */
};

/* receiver structure */
struct receiver {
    int listen_fd;
    struct watch listen_watch;

    bool running;
    int epoll_fd;

    struct wl_list client_list; /* struct client::link */
};

struct shm_buffer {
    struct wl_buffer *buffer;
    void *shm_data;
    int busy;
};

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    bool has_xrgb;
    struct ivi_application *ivi_application;

    struct wl_seat *seat;
    struct wl_pointer *wl_pointer;
    struct wl_keyboard *wl_keyboard;
    struct wl_touch *wl_touch;
    struct window *window;
    struct {
	    EGLDisplay dpy;
	    EGLContext ctx;
	    EGLConfig conf;

	    PFNEGLCREATEIMAGEKHRPROC create_image;
	    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
	    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_texture_2d;
    } egl;
    struct {
	    GLuint vertex_shader;
	    GLuint fragment_shader;
	    GLuint program_object;
	    GLuint texture;
    } gl;
};

struct window {
    struct display *display;
    struct {
	GLuint rotation_uniform;
	GLuint pos;
	GLuint col;
    } gl;
    int width, height;
    struct wl_surface *surface;
    struct ivi_surface *ivi_surface;
    struct shm_buffer buffers[2];
    struct shm_buffer *prev_buffer;
    struct wl_callback *callback;
    uint32_t window_frames;
    uint32_t window_benchmark_time;
    int wait;
    struct surface *receiver_surf;
    struct wl_egl_window *native;
    EGLSurface egl_surface;
    EGLImageKHR egl_img;
    struct seat *receiver_seat;
    struct pointer *receiver_pointer;
    bool ready;
    uint32_t id_ivisurf;
};


#endif
