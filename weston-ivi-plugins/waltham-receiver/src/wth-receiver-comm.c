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
**  PURPOSE   :  This file acts as interface to waltham IPC library           **
**                                                                            **
**                                                                            **
*******************************************************************************/

#include "wth-receiver-comm.h"

extern int wth_receiver_weston_main(struct window *);

extern void wth_receiver_weston_shm_attach(struct window *, uint32_t data_sz, void * data,
       int32_t width, int32_t height, int32_t stride, uint32_t format);
extern void wth_receiver_weston_shm_damage(struct window *);
extern void wth_receiver_weston_shm_commit(struct window *);

/*
 * utility functions
 */
static int
watch_ctl(struct watch *w, int op, uint32_t events)
{
    wth_verbose("%s >>> \n",__func__);
    struct epoll_event ee;

    ee.events = events;
    ee.data.ptr = w;
    wth_verbose(" <<< %s \n",__func__);
    return epoll_ctl(w->receiver->epoll_fd, op, w->fd, &ee);
}

static void
client_post_out_of_memory(struct client *c)
{
    wth_verbose("%s >>> \n",__func__);
    struct wth_display *disp;

    disp = wth_connection_get_display(c->connection);
    wth_object_post_error((struct wth_object *)disp, 1,
                  "out of memory");
    wth_verbose(" <<< %s \n",__func__);
}

/*
 * waltham surface implementation
 */
static void
surface_destroy(struct surface *surface)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("surface %p destroy\n", surface->obj);

    wthp_surface_free(surface->obj);
    wl_list_remove(&surface->link);
    free(surface);
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_destroy(struct wthp_surface *wthp_surface)
{
    wth_verbose("%s >>> \n",__func__);
    struct surface *surface = wth_object_get_user_data((struct wth_object *)wthp_surface);

    assert(wthp_surface == surface->obj);

    surface_destroy(surface);
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_attach(struct wthp_surface *wthp_surface,
              struct wthp_buffer *wthp_buffer, int32_t x, int32_t y)
{
    wth_verbose("%s >>> \n",__func__);
    struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);
    struct buffer *buf = NULL;

    buf = container_of(wthp_buffer, struct buffer, obj);

    if (surf->ivi_id != 0) {
        wth_receiver_weston_shm_attach(surf->shm_window,
               buf->data_sz,
               buf->data,
               buf->width,
               buf->height,
               buf->stride,
               buf->format);

        wthp_buffer_send_complete(wthp_buffer, 0);
    }
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_damage(struct wthp_surface *wthp_surface,
              int32_t x, int32_t y, int32_t width, int32_t height)
{
    wth_verbose("%s >>> \n",__func__);

    wth_verbose("surface %p damage(%d, %d, %d, %d)\n",
                wthp_surface, x, y, width, height);

    struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);

    if (surf->ivi_id != 0) {
        wth_receiver_weston_shm_damage(surf->shm_window);
    }
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_frame(struct wthp_surface *wthp_surface,
             struct wthp_callback *callback)
{
    wth_verbose("%s >>> \n",__func__);
    struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);
    wth_verbose("surface %p callback(%p)\n",wthp_surface, callback);

    surf->cb = callback;
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_set_opaque_region(struct wthp_surface *wthp_surface,
                 struct wthp_region *region)
{
    wth_verbose("surface %p set_opaque_region(%p)\n",
                wthp_surface, region);
}

static void
surface_handle_set_input_region(struct wthp_surface *wthp_surface,
                 struct wthp_region *region)
{
    wth_verbose("surface %p set_input_region(%p)\n",
                wthp_surface, region);
}

static void
surface_handle_commit(struct wthp_surface *wthp_surface)
{
    wth_verbose("%s >>> \n",__func__);

    struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);
    wth_verbose("commit %p\n",wthp_surface);

    if (surf->ivi_id != 0) {
        wth_receiver_weston_shm_commit(surf->shm_window);
    }
    wth_verbose(" <<< %s \n",__func__);
}

static void
surface_handle_set_buffer_transform(struct wthp_surface *wthp_surface,
                    int32_t transform)
{
    wth_verbose("surface %p et_buffer_transform(%d)\n",
                wthp_surface, transform);
}

static void
surface_handle_set_buffer_scale(struct wthp_surface *wthp_surface,
                int32_t scale)
{
    wth_verbose("surface %p set_buffer_scale(%d)\n",
                wthp_surface, scale);
}

static void
surface_handle_damage_buffer(struct wthp_surface *wthp_surface,
                 int32_t x, int32_t y, int32_t width, int32_t height)
{
    wth_verbose("surface %p damage_buffer(%d, %d, %d, %d)\n",
                wthp_surface, x, y, width, height);
}

static const struct wthp_surface_interface surface_implementation = {
    surface_handle_destroy,
    surface_handle_attach,
    surface_handle_damage,
    surface_handle_frame,
    surface_handle_set_opaque_region,
    surface_handle_set_input_region,
    surface_handle_commit,
    surface_handle_set_buffer_transform,
    surface_handle_set_buffer_scale,
    surface_handle_damage_buffer
};


/* BEGIN wthp_region implementation */

static void
buffer_handle_destroy(struct wthp_buffer *wthp_buffer)
{
	struct buffer *buf = wth_object_get_user_data((struct wth_object *)wthp_buffer);

	wthp_buffer_free(wthp_buffer);
	wl_list_remove(&buf->link);
	free(buf);
}

static const struct wthp_buffer_interface buffer_implementation = {
	buffer_handle_destroy
};

/* END wthp_region implementation */

/* BEGIN wthp_blob_factory implementation */

static void
blob_factory_create_buffer(struct wthp_blob_factory *blob_factory,
			   struct wthp_buffer *wthp_buffer, uint32_t data_sz, void *data,
			   int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	struct blob_factory *blob = wth_object_get_user_data((struct wth_object *)blob_factory);
	struct buffer *buffer;


	wth_verbose("wthp_blob_factory %p create_buffer(%p, %d, %p, %d, %d, %d, %d)\n",
		blob_factory, wthp_buffer, data_sz, data, width, height, stride, format);

	buffer = zalloc(sizeof *buffer);
	if (!buffer) {
		client_post_out_of_memory(blob->client);
		return;
	}

	wl_list_insert(&blob->client->buffer_list, &buffer->link);

	buffer->data_sz = data_sz;
	buffer->data = data;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->format = format;
	buffer->obj = wthp_buffer;

	wthp_buffer_set_interface(wthp_buffer, &buffer_implementation, buffer);
}

static const struct wthp_blob_factory_interface blob_factory_implementation = {
	blob_factory_create_buffer
};

static void
client_bind_blob_factory(struct client *c, struct wthp_blob_factory *obj)
{
	struct blob_factory *blob;

	blob = zalloc(sizeof *blob);
	if (!blob) {
		client_post_out_of_memory(c);
		return;
	}

	blob->obj = obj;
	blob->client = c;
	wl_list_insert(&c->compositor_list, &blob->link);

	wthp_blob_factory_set_interface(obj, &blob_factory_implementation,
					 blob);
  fprintf(stderr, "client %p bound wthp_blob_factory\n", c);
}

/*
 *  waltam ivi surface implementation
 */
static void
wthp_ivi_surface_destroy(struct wthp_ivi_surface * ivi_surface)
{
    wth_verbose("%s >>> \n",__func__);
    struct ivisurface *ivisurf = wth_object_get_user_data((struct wth_object *)ivi_surface);
    free(ivisurf);
    wth_verbose(" <<< %s \n",__func__);
}

static const struct wthp_ivi_surface_interface wthp_ivi_surface_implementation = {
    wthp_ivi_surface_destroy,
};


/*
 * waltham ivi application implementation
 */
static void
wthp_ivi_application_surface_create(struct wthp_ivi_application * ivi_application, uint32_t ivi_id,
                   struct wthp_surface * wthp_surface, struct wthp_ivi_surface *obj)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("ivi_application %p surface_create(%d, %p, %p)\n",
        ivi_application, ivi_id, wthp_surface, obj);
    struct surface *surface = wth_object_get_user_data((struct wth_object *)wthp_surface);
    wth_verbose("----------------------------------\n\n\n");
    wth_verbose("surface    [%p]\n", surface);
    wth_verbose("shm_window [%p]\n\n\n", surface->shm_window);
    wth_verbose("----------------------------------\n");

    surface->ivi_id = ivi_id + 100;
    surface->shm_window->id_ivisurf = surface->ivi_id;
    struct ivisurface *ivisurf;

    ivisurf = zalloc(sizeof *ivisurf);
    if (!ivisurf) {
        return;
    }

    ivisurf->obj = obj;
    ivisurf->surf = surface;

    wth_receiver_weston_main(surface->shm_window);

    while (!surface->shm_window->ready)
        usleep(1);

    wthp_ivi_surface_set_interface(obj, &wthp_ivi_surface_implementation,
                  ivisurf);
    wth_verbose(" <<< %s \n",__func__);
}

static const struct wthp_ivi_application_interface wthp_ivi_application_implementation = {
    wthp_ivi_application_surface_create,
};

static void
client_bind_wthp_ivi_application(struct client *c, struct wthp_ivi_application *obj)
{
    wth_verbose("%s >>> \n",__func__);

    struct application *app;

    app = zalloc(sizeof *app);
    if (!app) {
        client_post_out_of_memory(c);
        return;
    }

    app->obj = obj;
    app->client = c;
    wl_list_insert(&c->compositor_list, &app->link);

    wthp_ivi_application_set_interface(obj, &wthp_ivi_application_implementation,
                      app);
    wth_verbose("client %p bound wthp_ivi_application\n", c);
    wth_verbose(" <<< %s \n",__func__);
}

/*
 * APIs to send pointer events to waltham client
 */

void
waltham_pointer_enter(struct window *window, uint32_t serial,
                      wl_fixed_t sx, wl_fixed_t sy)
{
    wth_verbose("%s >>> \n",__func__);

    struct surface *surface = window->receiver_surf;
    struct seat *seat = window->receiver_seat;
    struct pointer *pointer = seat->pointer;

    wth_verbose("waltham_pointer_enter [%d]\n", window->receiver_surf->ivi_id);

    wthp_pointer_send_enter (pointer->obj, serial, surface->obj, sx, sy);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_pointer_leave(struct window *window, uint32_t serial)
{
    wth_verbose("%s >>> \n",__func__);
    struct surface *surface = window->receiver_surf;
    struct seat *seat = window->receiver_seat;
    struct pointer *pointer = seat->pointer;

    wth_verbose("waltham_pointer_leave [%d]\n", window->receiver_surf->ivi_id);

    wthp_pointer_send_leave (pointer->obj, serial, surface->obj);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_pointer_motion(struct window *window, uint32_t time,
                       wl_fixed_t sx, wl_fixed_t sy)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct pointer *pointer = seat->pointer;

    wthp_pointer_send_motion (pointer->obj, time, sx, sy);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_pointer_button(struct window *window, uint32_t serial,
               uint32_t time, uint32_t button,
               uint32_t state)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct pointer *pointer = seat->pointer;

    wthp_pointer_send_button (pointer->obj, serial, time, button, state);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_pointer_axis(struct window *window, uint32_t time,
             uint32_t axis, wl_fixed_t value)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct pointer *pointer = seat->pointer;

    wthp_pointer_send_axis (pointer->obj, time, axis, value);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

/*
 *  waltham pointer implementation
 */
static void
pointer_set_cursor(struct wthp_pointer *wthp_pointer, int32_t serial, struct wthp_surface *surface,
           int32_t hotspot_x, int32_t hotspot_y)
{
    struct pointer *pointer = wth_object_get_user_data((struct wth_object *)wthp_pointer);

    wth_verbose("wthp_pointer %p (%d, %p, %d, %d)\n",
        wthp_pointer, serial, surface, hotspot_x, hotspot_y);

}

static void
pointer_release(struct wthp_pointer *wthp_pointer)
{
    struct pointer *pointer = wth_object_get_user_data((struct wth_object *)wthp_pointer);

    wth_verbose("wthp_pointer %p\n",wthp_pointer);
}

static const struct wthp_pointer_interface pointer_implementation = {
    pointer_set_cursor,
    pointer_release
};

/*
 * APIs to send touch events to waltham client
 */

void
waltham_touch_down(struct window *window, uint32_t serial,
                   uint32_t time, int32_t id,
           wl_fixed_t x_w, wl_fixed_t y_w)
{
    wth_verbose("%s >>> \n",__func__);
    struct surface *surface = window->receiver_surf;
    struct seat *seat = window->receiver_seat;
    struct touch *touch = seat->touch;

    wth_verbose("touch_handle_down surface [%d]\n", surface->ivi_id);
    wthp_touch_send_down(touch->obj, serial, time, surface->obj, id, x_w, y_w);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_touch_up(struct window *window, uint32_t serial,
                 uint32_t time, int32_t id)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct touch *touch = seat->touch;

    wthp_touch_send_up(touch->obj, serial, time, id);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_touch_motion(struct window *window, uint32_t time,
             int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct touch *touch = seat->touch;

    wthp_touch_send_motion(touch->obj, time, id, x_w, y_w);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_touch_frame(struct window *window)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct touch *touch = seat->touch;

    wthp_touch_send_frame(touch->obj);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

void
waltham_touch_cancel(struct window *window)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat = window->receiver_seat;
    struct touch *touch = seat->touch;

    wthp_touch_send_cancel(touch->obj);

    wth_verbose(" <<< %s \n",__func__);
    return;
}

/*
 *  waltham touch implementation
 */
static void
touch_release(struct wthp_touch *wthp_touch)
{
    wth_verbose("%s >>> \n",__func__);
    struct touch *touch = wth_object_get_user_data((struct wth_object *)wthp_touch);

    wth_verbose("%p\n",wthp_touch);
}

static const struct wthp_touch_interface touch_implementation = {
    touch_release,
};

/*
 *  waltham seat implementation
 */
static void
seat_get_pointer(struct wthp_seat *wthp_seat, struct wthp_pointer *wthp_pointer)
{
    wth_verbose("%s >>> \n",__func__);

    wth_verbose("wthp_seat %p get_pointer(%p)\n",
        wthp_seat, wthp_pointer);

    struct seat *seat = wth_object_get_user_data((struct wth_object *)wthp_seat);
    struct pointer *pointer;

    pointer = zalloc(sizeof *pointer);
    if (!pointer) {
        client_post_out_of_memory(seat->client);
        return;
    }

    pointer->obj = wthp_pointer;
    pointer->seat = seat;
    seat->pointer = pointer;
    wl_list_insert(&seat->client->pointer_list, &pointer->link);

    wthp_pointer_set_interface(wthp_pointer, &pointer_implementation, pointer);
    wth_verbose(" <<< %s \n",__func__);
}

static void
seat_get_touch(struct wthp_seat *wthp_seat, struct wthp_touch *wthp_touch)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("wthp_seat %p get_touch(%p)\n",
        wthp_seat, wthp_touch);

    struct seat *seat = wth_object_get_user_data((struct wth_object *)wthp_seat);
    struct touch *touch;

    touch = zalloc(sizeof *touch);
    if (!touch) {
        client_post_out_of_memory(seat->client);
        return;
    }

    touch->obj = wthp_touch;
    touch->seat = seat;
    seat->touch = touch;
    wl_list_insert(&seat->client->touch_list, &touch->link);

    wthp_touch_set_interface(wthp_touch, &touch_implementation, touch);
    wth_verbose(" <<< %s \n",__func__);
}

static void
seat_release(struct wthp_seat *wthp_seat)
{
}

static const struct wthp_seat_interface seat_implementation = {
    seat_get_pointer,
    NULL,
    seat_get_touch,
    seat_release,
    NULL
};

static void
seat_send_updated_caps(struct seat *seat)
{
    wth_verbose("%s >>> \n",__func__);
    enum wthp_seat_capability caps = 0;

    caps |= WTHP_SEAT_CAPABILITY_POINTER;
    wth_verbose("WTHP_SEAT_CAPABILITY_POINTER %d\n", caps);

    caps |= WTHP_SEAT_CAPABILITY_TOUCH;
    wth_verbose("WTHP_SEAT_CAPABILITY_TOUCH %d\n", caps);

    wthp_seat_send_capabilities(seat->obj, caps);
    wth_verbose(" <<< %s \n",__func__);
}

static void
client_bind_seat(struct client *c, struct wthp_seat *obj)
{
    wth_verbose("%s >>> \n",__func__);
    struct seat *seat;

    seat = zalloc(sizeof *seat);
    if (!seat) {
        client_post_out_of_memory(c);
        return;
    }

    seat->obj = obj;
    seat->client = c;
    wl_list_insert(&c->seat_list, &seat->link);
    wth_verbose("wthp_seat object=%p and seat=%p\n",obj,seat);
    wthp_seat_set_interface(obj, &seat_implementation,
                seat);
    wth_verbose("client %p bound wthp_seat\n", c);
    seat_send_updated_caps(seat);
    wth_verbose(" <<< %s \n",__func__);
}

/*
 * waltham region implementation
 */

static void
region_destroy(struct region *region)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("region %p destroy\n", region->obj);

    wthp_region_free(region->obj);
    wl_list_remove(&region->link);
    free(region);
    wth_verbose(" <<< %s \n",__func__);
}

static void
region_handle_destroy(struct wthp_region *wthp_region)
{
    wth_verbose("%s >>> \n",__func__);
    struct region *region = wth_object_get_user_data((struct wth_object *)wthp_region);

    assert(wthp_region == region->obj);

    region_destroy(region);
    wth_verbose(" <<< %s \n",__func__);
}

static void
region_handle_add(struct wthp_region *wthp_region,
          int32_t x, int32_t y, int32_t width, int32_t height)
{
    wth_verbose("region %p add(%d, %d, %d, %d)\n",
        wthp_region, x, y, width, height);
}

static void
region_handle_subtract(struct wthp_region *wthp_region,
               int32_t x, int32_t y,
               int32_t width, int32_t height)
{
    wth_verbose("region %p subtract(%d, %d, %d, %d)\n",
        wthp_region, x, y, width, height);
}

static const struct wthp_region_interface region_implementation = {
    region_handle_destroy,
    region_handle_add,
    region_handle_subtract
};

/*
 * waltham compositor implementation
 */
static void
compositor_destroy(struct compositor *comp)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("%s: %p\n", __func__, comp->obj);

    wthp_compositor_free(comp->obj);
    wl_list_remove(&comp->link);
    free(comp);
    wth_verbose(" <<< %s \n",__func__);
}

static void
compositor_handle_create_surface(struct wthp_compositor *compositor,
                 struct wthp_surface *id)
{
    wth_verbose("%s >>> \n",__func__);
    struct compositor *comp = wth_object_get_user_data((struct wth_object *)compositor);
    struct client *client = comp->client;
    struct surface *surface;
    struct seat *seat, *tmp;

    wth_verbose("client %p create surface %p\n",
        comp->client, id);

    surface = zalloc(sizeof *surface);
    if (!surface) {
        client_post_out_of_memory(comp->client);
        return;
    }

    surface->obj = id;
    wl_list_insert(&comp->client->surface_list, &surface->link);

    wthp_surface_set_interface(id, &surface_implementation, surface);

    surface->shm_window = calloc(1, sizeof *surface->shm_window);
        if (!surface->shm_window)
        return;

    surface->shm_window->receiver_surf = surface;
    surface->shm_window->ready = false;
    surface->ivi_id = 0;

    wl_list_for_each_safe(seat, tmp, &client->seat_list, link) {
        surface->shm_window->receiver_seat = seat;
    }

    wth_verbose(" <<< %s \n",__func__);
}

static void
compositor_handle_create_region(struct wthp_compositor *compositor,
                struct wthp_region *id)
{
    wth_verbose("%s >>> \n",__func__);
    struct compositor *comp = wth_object_get_user_data((struct wth_object *)compositor);
    struct region *region;

    wth_verbose("client %p create region %p\n",
        comp->client, id);

    region = zalloc(sizeof *region);
    if (!region) {
        client_post_out_of_memory(comp->client);
        return;
    }

    region->obj = id;
    wl_list_insert(&comp->client->region_list, &region->link);

    wthp_region_set_interface(id, &region_implementation, region);
    wth_verbose(" <<< %s \n",__func__);
}

static const struct wthp_compositor_interface compositor_implementation = {
    compositor_handle_create_surface,
    compositor_handle_create_region
};

static void
client_bind_compositor(struct client *c, struct wthp_compositor *obj)
{
    wth_verbose("%s >>> \n",__func__);
    struct compositor *comp;

    comp = zalloc(sizeof *comp);
    if (!comp) {
        client_post_out_of_memory(c);
        return;
    }

    comp->obj = obj;
    comp->client = c;
    wl_list_insert(&c->compositor_list, &comp->link);

    wthp_compositor_set_interface(obj, &compositor_implementation,
                      comp);
    wth_verbose("client %p bound wthp_compositor\n", c);
    wth_verbose(" <<< %s \n",__func__);
}

/*
 * waltham registry implementation
 */
static void
registry_destroy(struct registry *reg)
{
    wth_verbose("%s >>> \n",__func__);
    wth_verbose("%s: %p\n", __func__, reg->obj);

    wthp_registry_free(reg->obj);
    wl_list_remove(&reg->link);
    free(reg);
    wth_verbose(" <<< %s \n",__func__);
}

static void
registry_handle_destroy(struct wthp_registry *registry)
{
    wth_verbose("%s >>> \n",__func__);
    struct registry *reg = wth_object_get_user_data((struct wth_object *)registry);

    registry_destroy(reg);
    wth_verbose(" <<< %s \n",__func__);
}

static void
registry_handle_bind(struct wthp_registry *registry,
             uint32_t name,
             struct wth_object *id,
             const char *interface,
             uint32_t version)
{
    wth_verbose("%s >>> \n",__func__);
    struct registry *reg = wth_object_get_user_data((struct wth_object *)registry);
    wth_verbose("Recieved registry : %s\n", interface);

    if (strcmp(interface, "wthp_compositor") == 0) {
        client_bind_compositor(reg->client, (struct wthp_compositor *)id);
    } else if (strcmp(interface, "wthp_blob_factory") == 0) {
		client_bind_blob_factory(reg->client, (struct wthp_blob_factory *)id);
		struct client *client = reg->client;
		struct seat *seat, *tmp,*get_seat;
		wl_list_for_each_safe(seat, tmp, &client->seat_list, link) {
			get_seat = seat;
		    }
		wth_verbose("get_seat : %p\n", get_seat);
		seat_send_updated_caps(get_seat);
    } else if (strcmp(interface, "wthp_ivi_application") == 0) {
        client_bind_wthp_ivi_application(reg->client, (struct wthp_ivi_application *)id);
    } else if (strcmp(interface, "wthp_seat") == 0) {
        client_bind_seat(reg->client, (struct wthp_seat *)id);
    } else {
        wth_object_post_error((struct wth_object *)registry, 0,
                      "%s: unknown name %u", __func__, name);
        wth_object_delete(id);
    }
    wth_verbose(" <<< %s \n",__func__);
}

static const struct wthp_registry_interface registry_implementation = {
    registry_handle_destroy,
    registry_handle_bind
};

/*
 * waltham display implementation
 */

static void
display_handle_client_version(struct wth_display *wth_display,
                  uint32_t client_version)
{
    wth_verbose("%s >>> \n",__func__);
    wth_object_post_error((struct wth_object *)wth_display, 0,
                  "unimplemented: %s", __func__);
    wth_verbose(" <<< %s \n",__func__);
}

static void
display_handle_sync(struct wth_display * wth_display, struct wthp_callback * callback)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *c = wth_object_get_user_data((struct wth_object *)wth_display);

    wth_verbose("Client %p requested wth_display.sync\n", c);
    wthp_callback_send_done(callback, 0);
    wthp_callback_free(callback);
    wth_verbose(" <<< %s \n",__func__);
}

static void
display_handle_get_registry(struct wth_display *wth_display,
                struct wthp_registry *registry)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *c = wth_object_get_user_data((struct wth_object *)wth_display);
    struct registry *reg;

    reg = zalloc(sizeof *reg);
    if (!reg) {
        client_post_out_of_memory(c);
        return;
    }

    reg->obj = registry;
    reg->client = c;
    wl_list_insert(&c->registry_list, &reg->link);
    wthp_registry_set_interface(registry,
                    &registry_implementation, reg);

    wthp_registry_send_global(registry, 1, "wthp_compositor", 4);
    wthp_registry_send_global(registry, 1, "wthp_ivi_application", 1);
    wthp_registry_send_global(registry, 1, "wthp_seat", 4);
    wthp_registry_send_global(registry, 1, "wthp_blob_factory", 4);
    wth_verbose(" <<< %s \n",__func__);
}

const struct wth_display_interface display_implementation = {
    display_handle_client_version,
    display_handle_sync,
    display_handle_get_registry
};

/*
 * functions to handle waltham client connections
 */
static void
connection_handle_data(struct watch *w, uint32_t events)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *c = container_of(w, struct client, conn_watch);
    int ret;

    if (events & EPOLLERR) {
        wth_error("Client %p errored out.\n", c);
        client_destroy(c);

        return;
    }

    if (events & EPOLLHUP) {
        wth_error("Client %p hung up.\n", c);
        client_destroy(c);

        return;
    }

    if (events & EPOLLOUT) {
        ret = wth_connection_flush(c->connection);
        if (ret == 0)
            watch_ctl(&c->conn_watch, EPOLL_CTL_MOD, EPOLLIN);
        else if (ret < 0 && errno != EAGAIN){
            wth_error("Client %p flush error.\n", c);
            client_destroy(c);

            return;
        }
    }

    if (events & EPOLLIN) {
        ret = wth_connection_read(c->connection);
        if (ret < 0) {
            wth_error("Client %p read error.\n", c);
            client_destroy(c);

            return;
        }

        ret = wth_connection_dispatch(c->connection);
        if (ret < 0 && errno != EPROTO) {
            wth_error("Client %p dispatch error.\n", c);
            client_destroy(c);

            return;
        }
    }
    wth_verbose(" <<< %s \n",__func__);
}

/**
* client_create
*
* Create new client connection
*
* @param srv                   receiver structure
* @param wth_connection        Waltham connection handle
* @return                      Pointer to client structure
*/
static struct client *
client_create(struct receiver *srv, struct wth_connection *conn)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *c;
    struct wth_display *disp;

    c = zalloc(sizeof *c);
    if (!c)
        return NULL;

    c->receiver = srv;
    c->connection = conn;

    c->conn_watch.receiver = srv;
    c->conn_watch.fd = wth_connection_get_fd(conn);
    c->conn_watch.cb = connection_handle_data;
    if (watch_ctl(&c->conn_watch, EPOLL_CTL_ADD, EPOLLIN) < 0) {
        free(c);
        return NULL;
    }

    wth_verbose("Client %p connected.\n", c);

    wl_list_insert(&srv->client_list, &c->link);

    wl_list_init(&c->registry_list);
    wl_list_init(&c->compositor_list);
    wl_list_init(&c->seat_list);
    wl_list_init(&c->pointer_list);
    wl_list_init(&c->touch_list);
    wl_list_init(&c->region_list);
    wl_list_init(&c->surface_list);
    wl_list_init(&c->buffer_list);

    disp = wth_connection_get_display(c->connection);
    wth_display_set_interface(disp, &display_implementation, c);

    wth_verbose(" <<< %s \n",__func__);
    return c;
}

/**
* client_destroy
*
* Destroy client connection
*
* @param names        struct client *c
* @param value        client data
* @return             none
*/
void
client_destroy(struct client *c)
{
    wth_verbose("%s >>> \n",__func__);
    struct region *region;
    struct compositor *comp;
    struct registry *reg;
    struct surface *surface;

    wth_verbose("Client %p disconnected.\n", c);

    /* clean up remaining client resources in case the client
     * did not.
     */
    wl_list_last_until_empty(region, &c->region_list, link)
        region_destroy(region);

    wl_list_last_until_empty(comp, &c->compositor_list, link)
        compositor_destroy(comp);

    wl_list_last_until_empty(reg, &c->registry_list, link)
        registry_destroy(reg);

    wl_list_last_until_empty(surface, &c->surface_list, link)
        surface_destroy(surface);

    wl_list_remove(&c->link);
    watch_ctl(&c->conn_watch, EPOLL_CTL_DEL, 0);
    wth_connection_destroy(c->connection);
    free(c);
    wth_verbose(" <<< %s \n",__func__);
}

/**
* receiver_flush_clients
*
* write all the pending requests from the clients to socket
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void
receiver_flush_clients(struct receiver *srv)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *c, *tmp;
    int ret;

    wl_list_for_each_safe(c, tmp, &srv->client_list, link) {
            /* Flush out buffered requests. If the Waltham socket is
             * full, poll it for writable too.
            */
        ret = wth_connection_flush(c->connection);
        if (ret < 0 && errno == EAGAIN) {
            watch_ctl(&c->conn_watch, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
        } else if (ret < 0) {
            perror("Connection flush failed");
            client_destroy(c);
            return;
        }
    }

    wth_verbose(" <<< %s \n",__func__);

}

/**
* receiver_accept_client
*
* Accepts new waltham client connection and instantiates client structure
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void
receiver_accept_client(struct receiver *srv)
{
    wth_verbose("%s >>> \n",__func__);
    struct client *client;
    struct wth_connection *conn;
    struct sockaddr_in addr;
    socklen_t len;

    len = sizeof addr;
    conn = wth_accept(srv->listen_fd, (struct sockaddr *)&addr, &len);
    if (!conn) {
        wth_error("Failed to accept a connection.\n");
        return;
    }

    client = client_create(srv, conn);
    if (!client) {
        wth_error("Failed client_create().\n");
        return;
    }
    wth_verbose(" <<< %s \n",__func__);
}
