/*
 * Copyright (C) 2017 Advanced Driver Information Technology GmbH, Advanced Driver Information Technology Corporation, Robert Bosch GmbH, Robert Bosch Car Multimedia GmbH, DENSO Corporation
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

#ifndef TRANSMITTER_WALTHAM_RENDERER_H_
#define TRANSMITTER_WALTHAM_RENDERER_H_

struct waltham_renderer_interface {
	int (*display_create)(struct weston_transmitter_output *output);
};

struct gst_settings {
	int width;
	int height;
	int bitrate;
	char *ip;
	int port;
};

#endif /* TRANSMITTER_WALTHAM_RENDERER_H_ */
