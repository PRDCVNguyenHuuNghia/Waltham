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
 **                                                                              **
 **  TARGET    : linux                                                           **
 **                                                                              **
 **  PROJECT   : waltham-receiver                                                **
 **                                                                              **
 **  PURPOSE   : This file is acts as interface to weston compositor at receiver **
 **  side                                                                        **
 **                                                                              **
 *******************************************************************************/

#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>
#include <gst/gst.h>
#include <GL/gl.h>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>
#include <pthread.h>
#include <gst/wayland/wayland.h>
#include <gst/video/videooverlay.h>

#include <xf86drm.h>
#include <drm.h>
#include <drm_fourcc.h>

#include "wth-receiver-comm.h"
#include "os-compatibility.h"
#include "ivi-application-client-protocol.h"
#include "bitmap.h"
static int running = 1;

typedef struct _GstAppContext
{
	GMainLoop *loop;
	GstBus *bus;
	GstElement *pipeline;
	GstElement *sink;
	struct display *display;
	struct window *window;
	GstVideoInfo info;
}GstAppContext;

static const gchar *vertex_shader_str =
"attribute vec4 a_position;   \n"
"attribute vec2 a_texCoord;   \n"
"varying vec2 v_texCoord;     \n"
"void main()                  \n"
"{                            \n"
"   gl_Position = a_position; \n"
"   v_texCoord = a_texCoord;  \n"
"}                            \n";

static const gchar *fragment_shader_str =
"#ifdef GL_ES                                          \n"
"precision mediump float;                              \n"
"#endif                                                \n"
"varying vec2 v_texCoord;                              \n"
"uniform sampler2D tex;                                \n"
"void main()                                           \n"
"{                                                     \n"
"vec2 uv;                                              \n"
"uv = v_texCoord.xy;                                   \n"
"vec4 c = texture2D(tex, uv);                          \n"
"gl_FragColor = c;                                     \n"
"}                                                     \n";

/*
 * pointer callbcak functions
 */
	static void
pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *wl_surface,
		wl_fixed_t sx, wl_fixed_t sy)
{
	wth_verbose("%s >>> \n",__func__);

	wth_verbose("data [%p]\n", data);

	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_enter(window, serial, sx, sy);

	wth_verbose(" <<< %s \n",__func__);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface)
{
	wth_verbose("%s >>> \n",__func__);

	wth_verbose("data [%p]\n", data);

	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_leave(window, serial);

	wth_verbose(" <<< %s \n",__func__);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_motion(window, time, sx, sy);

	wth_verbose(" <<< %s \n",__func__);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_button(window, serial, time, button, state);

	wth_verbose(" <<< %s \n",__func__);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_axis(window, time, axis, value);

	wth_verbose(" <<< %s \n",__func__);
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

/*
 * touch callbcak functions
 */

static void
touch_handle_down(void *data, struct wl_touch *touch, uint32_t serial,
		uint32_t time, struct wl_surface *surface, int32_t id,
		wl_fixed_t x_w, wl_fixed_t y_w)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	int x = (int)wl_fixed_to_double(x_w);
	int y = (int)wl_fixed_to_double(y_w);
	wth_verbose("%p x %d y %d\n",window, x, y);

	waltham_touch_down(window, serial, time, id, x_w, y_w);

	wth_verbose(" <<< %s \n",__func__);
}

static void
touch_handle_up(void *data, struct wl_touch *touch, uint32_t serial,
		uint32_t time, int32_t id)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_up(window, serial, time, id);
}

static void
touch_handle_motion(void *data, struct wl_touch *touch, uint32_t time,
		int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_motion(window, time, id, x_w, y_w);

	wth_verbose(" <<< %s \n",__func__);
}

static void
touch_handle_frame(void *data, struct wl_touch *touch)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_frame(window);

	wth_verbose(" <<< %s \n",__func__);
}

static void
touch_handle_cancel(void *data, struct wl_touch *touch)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_cancel(window);

	wth_verbose(" <<< %s \n",__func__);
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel
};

/*
 * seat callback
 */
static void
seat_capabilities(void *data, struct wl_seat *wl_seat, enum wl_seat_capability caps)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display = data;

	wth_verbose("caps = %d\n", caps);

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !display->wl_pointer)
	{
		wth_verbose("WL_SEAT_CAPABILITY_POINTER\n");
		display->wl_pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_set_user_data(display->wl_pointer, display);
		wl_pointer_add_listener(display->wl_pointer, &pointer_listener, display);
		wl_display_roundtrip(display->display);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && display->wl_pointer) {
		wth_verbose("!WL_SEAT_CAPABILITY_POINTER\n");
		wl_pointer_destroy(display->wl_pointer);
		display->wl_pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !display->wl_touch)
	{
		wth_verbose("WL_SEAT_CAPABILITY_TOUCH\n");
		display->wl_touch = wl_seat_get_touch(wl_seat);
		wl_touch_set_user_data(display->wl_touch, display);
		wl_touch_add_listener(display->wl_touch, &touch_listener, display);
		wl_display_roundtrip(display->display);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && display->wl_touch) {
		wth_verbose("!WL_SEAT_CAPABILITY_TOUCH\n");
		wl_touch_destroy(display->wl_touch);
		display->wl_touch = NULL;
	}

	wth_verbose(" <<< %s \n",__func__);
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities,
	NULL
};

static void
add_seat(struct display *display, uint32_t id, uint32_t version)
{
	wth_verbose("%s >>> \n",__func__);

	display->wl_pointer = NULL;
	display->wl_touch = NULL;
	display->wl_keyboard = NULL;
	display->seat = wl_registry_bind(display->registry, id,
			&wl_seat_interface, 1);
	wl_seat_add_listener(display->seat, &seat_listener, display);
	wth_verbose(" <<< %s \n",__func__);
}

gboolean bus_message(GstBus *bus, GstMessage *message, gpointer p)
{
	GstAppContext *gstctx = p;

	fprintf(stderr, "mesage: %s\n", GST_MESSAGE_TYPE_NAME(message));

	switch( GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR:
	{
		GError *err;
		gchar *debug;

		gst_message_parse_error(message, &err, &debug);
		g_print("ERROR: %s\n", err->message);

		g_error_free(err);
		g_free(debug);
		g_main_loop_quit(gstctx->loop);
		break;
	}

	case GST_MESSAGE_STATE_CHANGED:
	{
		GstState oldstate, newstate;

		gst_message_parse_state_changed(message, &oldstate, &newstate, NULL);
		fprintf(stderr, "#%s state changed\n", GST_MESSAGE_SRC_NAME(message));
		switch (newstate){
		case GST_STATE_NULL:
			fprintf(stderr, "%s: state is NULL\n", GST_MESSAGE_SRC_NAME(message));
			break;
		case GST_STATE_READY:
			fprintf(stderr, "%s: state is READY\n", GST_MESSAGE_SRC_NAME(message));
			break;
		case GST_STATE_PAUSED:
			fprintf(stderr, "%s: state is PAUSED\n", GST_MESSAGE_SRC_NAME(message));
			break;
		case GST_STATE_PLAYING:
			fprintf(stderr, "%s: state is PLAYING\n", GST_MESSAGE_SRC_NAME(message));
			break;
		}
		break;
	}
	default:
		fprintf(stderr, "Unhandled message\n");
		break;
	}
	fprintf(stderr, "-----------------\n");
}

/*
 *  registry callback
 */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application =
			wl_registry_bind(registry, id,
					 &ivi_application_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		add_seat(d, id, version);
	}
	wth_verbose(" <<< %s \n",__func__);
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name)
{
	/* stub */
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

wth_receiver_weston_shm_attach(struct window *window, uint32_t data_sz, void * data,
		int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	/* stub */
}
void
wth_receiver_weston_shm_damage(struct window *window)
{
	/* stub */
}

void
wth_receiver_weston_shm_commit(struct window *window)
{
	/* stub */
}

static struct display *
create_display(void)
{
	wth_verbose("%s >>> \n",__func__);

	struct display *display;

	display = malloc(sizeof *display);
	if (display == NULL) {
		wth_error("out of memory\n");
		exit(1);
	}
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->has_xrgb = false;
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
			&registry_listener, display);
	wl_display_roundtrip(display->display);

	wth_verbose(" <<< %s \n",__func__);
	return display;
}

static void
destroy_display(struct display *display)
{
	wth_verbose("%s >>> \n",__func__);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	wl_registry_destroy(display->registry);
	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);

	wth_verbose(" <<< %s \n",__func__);
}

/*
 * ivi surface callback
 */
static void
handle_ivi_surface_configure(void *data, struct ivi_surface *ivi_surface,
		int32_t width, int32_t height)
{
	/* Simple-shm is resizable */
}

static const struct ivi_surface_listener ivi_surface_listener = {
	handle_ivi_surface_configure,
};

static void
init_egl(struct display *display)
{
	wth_verbose("%s >>> \n",__func__);
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint major, minor, count;
	EGLBoolean ret;

	display->egl.dpy = eglGetDisplay(display->display);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			&display->egl.conf, 1, &count);
	assert(ret && count >= 1);

	display->egl.ctx = eglCreateContext(display->egl.dpy,
			display->egl.conf,
			EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

	eglSwapInterval(display->egl.dpy, 1);

	display->egl.create_image =
		(void *) eglGetProcAddress("eglCreateImageKHR");
	assert(display->egl.create_image);

	display->egl.image_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	assert(display->egl.image_texture_2d);

	display->egl.destroy_image =
		(void *) eglGetProcAddress("eglDestroyImageKHR");
	assert(display->egl.destroy_image);
	wth_verbose("%s <<< \n",__func__);
}

GLuint load_shader(GLenum type, const char *shaderSrc)
{
	wth_verbose("%s >>> \n",__func__);
	GLuint shader;
	GLint compiled;

	/* Create the shader object */
	shader = glCreateShader(type);
	if (shader == 0)
	{
		printf("\n Failed to create shader \n");
		return 0;
	}
	/* Load the shader source */
	glShaderSource(shader, 1, &shaderSrc, NULL);
	/* Compile the shader */
	glCompileShader(shader);
	/* Check the compile status */
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc (sizeof(char) * infoLen );
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader:%s\n",infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

void init_gl(struct display *display)
{
	wth_verbose("%s >>> \n",__func__);
	GLint linked;

	/* load vertext/fragment shader */
	display->gl.vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_shader_str);
	display->gl.fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_shader_str);

	/* Create the program object */
	display->gl.program_object = glCreateProgram();
	if (display->gl.program_object == 0)
	{
		fprintf(stderr, "error program object\n");
		return;
	}

	glAttachShader(display->gl.program_object, display->gl.vertex_shader);
	glAttachShader(display->gl.program_object, display->gl.fragment_shader);
	/* Bind vPosition to attribute 0 */
	glBindAttribLocation(display->gl.program_object, 0, "a_position");
	/* Link the program */
	glLinkProgram(display->gl.program_object);
	/* Check the link status */
	glGetProgramiv(display->gl.program_object, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		GLint infoLen = 0;
		glGetProgramiv(display->gl.program_object, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(display->gl.program_object, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(display->gl.program_object);
	}

	glGenTextures(1, &display->gl.texture);

	return;
}

static void
create_surface(struct window *window)
{
	wth_verbose("%s >>> \n",__func__);
	struct display *display = window->display;
	int ret;

	window->surface = wl_compositor_create_surface(display->compositor);
	assert(window->surface);

	window->native = wl_egl_window_create(window->surface,
					      window->width, window->height);
	assert(window->native);

	window->egl_surface = eglCreateWindowSurface(display->egl.dpy,
						     display->egl.conf,
						     (NativeWindowType)window->native, NULL);

	wl_display_roundtrip(display->display);
	if (display->ivi_application ) {
		uint32_t id_ivisurf = window->id_ivisurf;
		window->ivi_surface =
			ivi_application_surface_create(display->ivi_application,
						       id_ivisurf, window->surface);
		if (window->ivi_surface == NULL) {
			wth_error("Failed to create ivi_client_surface\n");
			abort();
		}

		ivi_surface_add_listener(window->ivi_surface,
				&ivi_surface_listener, window);
	} else {
		assert(0);
	}
	ret = eglMakeCurrent(display->egl.dpy, window->egl_surface,
			     window->egl_surface, display->egl.ctx);
	assert(ret == EGL_TRUE);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(display->egl.dpy, window->egl_surface);
	wth_verbose(" <<< %s \n",__func__);
}

static void
create_window(struct window *window, struct display *display, int width, int height)
{
	wth_verbose("%s >>> \n",__func__);

	window->callback = NULL;

	window->display = display;
	window->width = width;
	window->height = height;
	window->window_frames = 0;
	window->window_benchmark_time = 0;

	create_surface(window);

	wth_verbose(" <<< %s \n",__func__);
	return;
}

static void
destroy_window(struct window *window)
{
	wth_verbose("%s >>> \n",__func__);

	if (window->callback)
		wl_callback_destroy(window->callback);

	if (window->buffers[0].buffer)
		wl_buffer_destroy(window->buffers[0].buffer);
	if (window->buffers[1].buffer)
		wl_buffer_destroy(window->buffers[1].buffer);

	wl_surface_destroy(window->surface);
	free(window);

	wth_verbose(" <<< %s \n",__func__);
}

static void
signal_int(int signum)
{
	running = 0;
}

void *stream_thread(void *data)
{
	wth_verbose("%s >>> \n",__func__);
	GMainLoop *loop = data;
	g_main_loop_run(loop);
	wth_verbose(" <<< %s \n",__func__);
}

static GstPadProbeReturn
pad_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	wth_verbose("%s >>> \n",__func__);
	GstAppContext *dec = user_data;
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps *caps;

	(void)pad;

	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	gst_event_parse_caps(event, &caps);

	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}

	if (!gst_video_info_from_caps(&dec->info, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}

	dec->window->width=GST_VIDEO_INFO_WIDTH(&dec->info);
	dec->window->height=GST_VIDEO_INFO_HEIGHT(&dec->info);
	gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY (dec->sink),0,0,dec->window->width,dec->window->height);
	wl_surface_commit(dec->window->surface);

	wth_verbose(" <<< %s \n",__func__);
	return GST_PAD_PROBE_OK;
}

/**
 * wth_receiver_weston_main
 *
 * This is the main function which will handle connection to the compositor at receiver side
 *
 * @param names        void *data
 * @param value        struct window data
 * @return             0 on success, -1 on error
 */
int
wth_receiver_weston_main(struct window *window)
{
	wth_verbose("%s >>> \n",__func__);

	struct sigaction sigint;
	pthread_t pthread;
	GstAppContext gstctx;
	int ret = 0;
	GError *gerror = NULL;
	char * pipe = NULL;
	FILE *pFile;
	long lSize;
	size_t res;
	GstContext *context;

	memset(&gstctx, 0, sizeof(gstctx));
	/* Initialization for window creation */
	gstctx.display = create_display();
	init_egl(gstctx.display);
	/* ToDo: fix the hardcoded value of width, height */
	create_window(window, gstctx.display,1920,1080);
	init_gl(gstctx.display);
	gstctx.window = window;

	gstctx.display->window = window;

	wth_verbose("display %p\n", gstctx.display);
	wth_verbose("display->window %p\n", gstctx.display->window);
	wth_verbose("window %p\n", window);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* create gstreamer pipeline */
	gst_init(NULL, NULL);
	gstctx.loop = g_main_loop_new(NULL, FALSE);

	/* Read pipeline from file */
	pFile = fopen ( "/etc/xdg/weston/receiver_pipeline.cfg" , "rb" );
	if (pFile==NULL){
		fprintf(stderr, "failed to open file\n");
		return -1;
	}

	/* obtain file size */
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);

	/* allocate memory to contain the whole file */
	pipe = (char*) zalloc (sizeof(char)*lSize);
	if (pipe == NULL){
		fprintf(stderr,"Cannot allocate memory\n");
		return -1;
	}

	/* copy the file into the buffer */
	res = fread (pipe,1,lSize,pFile);
	if (res != lSize){
		fprintf(stderr,"File read error\n");
		return -1;
	}

	wth_verbose("Gst Pipeline=%s",pipe);
	/* close file */
	fclose (pFile);

	/* parse the pipeline */
	gstctx.pipeline = gst_parse_launch(pipe, &gerror);

	if(!gstctx.pipeline)
		fprintf(stderr,"Could not create gstreamer pipeline.\n");
	free(pipe);

	gstctx.bus = gst_pipeline_get_bus((GstPipeline*)((void*)gstctx.pipeline));
	gst_bus_add_watch(gstctx.bus, bus_message, &gstctx);
	fprintf(stderr, "registered bus signal\n");

	/* get sink element */
	gstctx.sink = gst_bin_get_by_name(GST_BIN(gstctx.pipeline), "sink");
	/* get display context */
	context = gst_wayland_display_handle_context_new(gstctx.display->display);
	/* set external display from context to sink */
	gst_element_set_context(gstctx.sink,context);
	/* Attach existing surface to sink */
	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY (gstctx.sink),window->surface);

	gst_pad_add_probe(gst_element_get_static_pad(gstctx.sink, "sink"),
			GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
			pad_probe, &gstctx, NULL);

	fprintf(stderr, "set state as playing\n");
	gst_element_set_state((GstElement*)((void*)gstctx.pipeline), GST_STATE_PLAYING);


	pthread_create(&pthread, NULL, &stream_thread, gstctx.loop);

	fprintf(stderr, "rendering part\n");

	wth_verbose("in render loop\n");
	while (running && ret != -1)
		ret=wl_display_dispatch_pending(gstctx.display->display);


	wth_verbose("wth_receiver_gst_main exiting\n");

	if (window->display->ivi_application) {
		ivi_surface_destroy(window->ivi_surface);
		ivi_application_destroy(window->display->ivi_application);
	}

	gst_element_set_state((GstElement*)((void*)gstctx.pipeline), GST_STATE_NULL);
	destroy_window(window);
	destroy_display(gstctx.display);

	wth_verbose(" <<< %s \n",__func__);

	return 0;
}
