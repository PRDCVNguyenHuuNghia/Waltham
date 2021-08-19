/*
 * Copyright (C) 2017 Advanced Driver Information Technology Joint Venture GmbH
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

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "compositor.h"
#include "compositor-drm.h"
#include "plugin-registry.h"

#include "plugin.h"
#include "transmitter_api.h"
#include "waltham-renderer.h"

/** @file
 *
 * This is an implementation of a remote output.
 *
 * A remote output must not be accepted as an argument to:
 * - wl_shell_surface.set_fullscreen
 * - wl_shell_surface.set_maximized
 * - zwp_fullscreen_shell_v1.present_surface
 * - zwp_fullscreen_shell_v1.present_surface_for_mode
 * - zwp_input_panel_surface_v1.set_toplevel
 * - xdg_surface.set_fullscreen
 *
 * If a remote output is an argument to the above or similar requests,
 * it should have the same effect as NULL if possible.
 *
 * @todo Should we instead accept the argument and have it start remoting
 * automatically? That would be shell-specific.
 *
 * In ivi-shell's case, only zwp_input_panel_surface_v1.set_toplevel is
 * reachable from keyboard.c. That just blindly uses whatever the first
 * output happens to be, so there is no need to check for now.
 *
 * @todo Add weston_output_set_remote() which sets weston_output::is_remote
 * to true and inits weston_output::link. This should be made mutually
 * exclusive with weston_compositor_add_output().
 */

static struct waltham_renderer_interface *waltham_renderer;

static char *
make_model(struct weston_transmitter_remote *remote, int name)
{
	char *str;

	if (asprintf(&str, "transmitter-%s:%s-%d", remote->addr, remote->port, name) < 0)
		return NULL;

	return str;
}

static int
make_mode_list(struct wl_list *list,
	       const struct weston_transmitter_output_info *info)
{
	struct weston_mode *mode;

	mode = zalloc(sizeof *mode);
	if (!mode)
		return -1;

	*mode = info->mode;
	wl_list_insert(list->prev, &mode->link);

	return 0;
}

static struct weston_mode *
get_current_mode(struct wl_list *mode_list)
{
	struct weston_mode *mode;

	wl_list_for_each(mode, mode_list, link)
		if (mode->flags & WL_OUTPUT_MODE_CURRENT)
			return mode;

	assert(0);
	return NULL;
}

static void
free_mode_list(struct wl_list *mode_list)
{
	struct weston_mode *mode;

	while (!wl_list_empty(mode_list)) {
		mode = wl_container_of(mode_list->next, mode, link);

		wl_list_remove(&mode->link);
		free(mode);
	}
}

static int
transmitter_output_disable(struct weston_output *base)
{
	struct weston_transmitter_output *output = wl_container_of(base, output, base);
	wl_event_source_remove(output->finish_frame_timer);
	return 0;
}

void
transmitter_output_destroy(struct weston_transmitter_output *output)
{
	wl_list_remove(&output->link);

	struct weston_head *head=weston_output_get_first_head(&output->base);
	free_mode_list(&output->base.mode_list);
	weston_head_release(head);
	free(head);
	transmitter_output_disable(&output->base);
	weston_output_release(&output->base);
	free(output);
}

static void
transmitter_output_destroy_(struct weston_output *base)
{
	struct weston_transmitter_output *output = wl_container_of(base, output, base);

	transmitter_output_destroy(output);
}

static int
transmitter_output_finish_frame_handler(void *data)
{
	struct weston_transmitter_output *output = data;
	struct timespec now;
	weston_compositor_read_presentation_clock(output->base.compositor, &now);
	weston_output_finish_frame(&output->base, &now, 0);
	return 0;
}

static void
transmitter_start_repaint_loop(struct weston_output *base)
{
	struct weston_transmitter_output *output = wl_container_of(base, output, base);
	weston_output_finish_frame(&output->base,NULL, WP_PRESENTATION_FEEDBACK_INVALID);
}

static int
transmitter_output_repaint(struct weston_output *base,
			   pixman_region32_t *damage,void *repaint_data)
{
	struct weston_transmitter_output* output = wl_container_of(base, output, base);
	struct weston_transmitter_remote* remote = output->remote;
	struct weston_transmitter* txr = remote->transmitter;
	struct weston_transmitter_api* transmitter_api =
		weston_get_transmitter_api(txr->compositor);
	struct weston_transmitter_surface* txs;
	struct weston_compositor *compositor = base->compositor;
	struct weston_view *view;
	bool found_output = false;
	struct timespec ts;

	struct weston_drm_output_api *api =
		weston_plugin_api_get(txr->compositor,  WESTON_DRM_OUTPUT_API_NAME, sizeof(api));

	/*
	 * Pick up weston_view in transmitter_output and check weston_view's surface
	 * If the surface hasn't been conbined to weston_transmitter_surface,
	 * then call push_to_remote.
	 * If the surface has already been combined, call gather_state.
	 */
	if (wl_list_empty(&compositor->view_list))
		goto out;

	if (remote->status != WESTON_TRANSMITTER_CONNECTION_READY)
		goto out;

	wl_list_for_each_reverse(view, &compositor->view_list, link) {
		bool found_surface = false;
		if (view->output == &output->base && (view->surface->width >= 64 && view->surface->height >= 64)) {
			found_output = true;
			wl_list_for_each(txs, &remote->surface_list, link) {
				if (txs->surface == view->surface) {
					found_surface = true;
					if (!txs->wthp_surf)
						transmitter_api->surface_push_to_remote
							(view->surface, remote, NULL);

					output->renderer->dmafd =
						api->get_dma_fd_from_view(&output->base, view, &output->renderer->buf_stride);
					if(output->renderer->dmafd < 0) {
						weston_log("Failed to get dmafd\n");
						goto out;
					}

					/*
					 * Updating the width x height
					 * from surface to gst-recorder
					 */
					output->renderer->surface_width
						= view->surface->width;
					output->renderer->surface_height
						= view->surface->height;

					output->renderer->repaint_output(output);
					output->renderer->dmafd = NULL;
					transmitter_api->surface_gather_state(txs);
					weston_buffer_reference(&view->surface->buffer_ref, NULL);
					break;
				}
			}
			if (!found_surface){
				txs = transmitter_api->surface_push_to_remote(view->surface,
									remote, NULL);
				output->renderer->dmafd =
						api->get_dma_fd_from_view(&output->base, view, &output->renderer->buf_stride);
				if (output->renderer->dmafd < 0) {
					weston_log("Failed to get dmafd\n");
					goto out;
				}
				output->renderer->surface_width = view->surface->width;
				output->renderer->surface_height = view->surface->height;

				output->renderer->repaint_output(output);
				output->renderer->dmafd = NULL;
				transmitter_api->surface_gather_state(txs);
				weston_buffer_reference(&view->surface->buffer_ref, NULL);
				break;
			}
		}
	}
	if (!found_output)
		goto out;

	wl_event_source_timer_update(output->finish_frame_timer,1);
	return 0;

out:
	wl_event_source_timer_update(output->finish_frame_timer,1);
	return 0;
}

static void
transmitter_assign_planes(struct weston_output *base,void *repaint_data) {
	/*
	 * This function prevents compositor releasing buffer early.
	 */
	struct weston_transmitter_output* output = wl_container_of(base, output, base);
	struct weston_transmitter_remote* remote = output->remote;
	struct weston_transmitter_surface* txs;
	struct weston_compositor *compositor = base->compositor;
	struct weston_view *view;

	wl_list_for_each_reverse(view, &compositor->view_list, link) {
		if (view->output == &output->base && (view->surface->width >= 64 && view->surface->height >= 64)) {
			view->surface->keep_buffer = true;
		}
	}
}


static int
transmitter_output_enable(struct weston_output *base)
{
	struct weston_transmitter_output *output = wl_container_of(base, output, base);
	struct wl_event_loop *loop;
	if (!output) {
		weston_log("No weston output found\n");
		return -1;
	}
	output->base.assign_planes = transmitter_assign_planes;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	loop = wl_display_get_event_loop(base->compositor->wl_display);
	output->finish_frame_timer =
			wl_event_loop_add_timer(loop,
						transmitter_output_finish_frame_handler,
						output);
	return 0;
}

/* FIXME:This is a dummy call we just return 0(success)*/
int transmitter_output_attach_head(struct weston_output *output,
		   struct weston_head *head)
{
	weston_log("%s is called\n",__func__);
	return 0;
}

/* FIXME:This is a dummy call we just return 0(success)*/
int transmitter_output_detach_head(struct weston_output *output,
		   struct weston_head *head)
{
	weston_log("%s is called\n",__func__);
	return 0;
}

int
transmitter_remote_create_output(struct weston_transmitter_remote *remote,
				 const struct weston_transmitter_output_info *info)
{
	struct weston_transmitter_output *output;
	struct weston_transmitter *txr = remote->transmitter;
	struct weston_head *head;
	const char *make = strdup(WESTON_TRANSMITTER_OUTPUT_MAKE);
	const char *model = make_model(remote, 1);
	const char *serial_number = strdup("0");
	const char *connector_name = make_model(remote, 1);

	head=zalloc(sizeof *head);
	if (!head){
		weston_log("allocation failed for head\n");
		return -1;
	}

	output = zalloc(sizeof *output);
	if (!output)
		return -1;

	output->parent.draw_initial_frame = true;

	weston_head_init(head,connector_name);
	weston_head_set_subpixel(head, info->subpixel);
	weston_head_set_monitor_strings(head, make, model, serial_number);

	head->compositor=remote->transmitter->compositor;

	/* x and y is fake value */
	wl_list_init(&output->base.mode_list);
	output->base.name = make_model(remote, 1);
	/* WL_OUTPUT_MODE_CURRENT already set */
	weston_output_init(&output->base, remote->transmitter->compositor,output->base.name);
	if (make_mode_list(&output->base.mode_list, info) < 0)
		goto fail;

	output->base.current_mode = get_current_mode(&output->base.mode_list);
	output->base.height = output->base.current_mode->height;
	output->base.width = output->base.current_mode->width;

	output->base.enable = transmitter_output_enable;
	output->base.start_repaint_loop = transmitter_start_repaint_loop;
	output->base.repaint = transmitter_output_repaint;
	output->base.destroy = transmitter_output_destroy_;
	output->base.disable = transmitter_output_disable;
	output->base.assign_planes = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;
	output->base.gamma_size = 0;
	output->base.set_gamma = NULL;

	output->base.native_mode = output->base.current_mode;
	output->base.native_scale = output->base.current_scale;
	output->base.scale = 1;
	output->base.transform = WL_OUTPUT_TRANSFORM_NORMAL;

	output->base.attach_head = transmitter_output_attach_head;
	output->base.detach_head = transmitter_output_detach_head;

	output->remote = remote;
	wl_list_insert(&remote->output_list, &output->link);

	if (txr->waltham_renderer->display_create(output) < 0) {
		weston_log("Failed to create waltham renderer display \n");
		return -1;
	}

	if(!weston_output_attach_head(&output->base,head)){
		weston_log("Weston head attached successfully to output\n");
	}

	if(weston_output_enable(&output->base)<0){
		weston_log("Failed to enable weston output\n");
	}

	return 0;

fail:
	free_mode_list(&output->base.mode_list);
	free(head);
	free(output);

	return -1;
}
