/*
 * rofi
 *
 * MIT/X11 License
 * Copyright © 2013-2020 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef ROFI_WAYLAND_H
#define ROFI_WAYLAND_H

#include <cairo.h>
#include <glib.h>

#include "display.h"

typedef struct _display_buffer_pool display_buffer_pool;
display_buffer_pool *display_buffer_pool_new(gint width, gint height);
void display_buffer_pool_free(display_buffer_pool *pool);

cairo_surface_t *display_buffer_pool_get_next_buffer(display_buffer_pool *pool);
void display_surface_commit(cairo_surface_t *surface);

/**
 * Usable area of the output rofi is displayed on, in logical pixels.
 *
 * @returns FALSE if the size is not known yet.
 */
gboolean display_get_output_dimensions(int *width, int *height);
void display_set_surface_dimensions(int width, int height, int x_margin,
                                    int y_margin, int loc);

/**
 * Whether clicks outside of the menu should be caught to dismiss rofi. This
 * requires a surface covering the output, which only the layer shell can do.
 */
gboolean display_capture_outside_clicks(void);

/**
 * Set the window title, if the shell in use has one.
 */
void display_set_window_title(const char *title);

void wayland_display_set_cursor_type(RofiCursorType type);

/**
 * @param width  Width assigned by the compositor, 0 if it is ours to pick.
 * @param height Height assigned by the compositor, 0 if it is ours to pick.
 *
 * Record the size the compositor dictated for the window.
 */
void wayland_rofi_view_set_configured_size(int width, int height);

/**
 * Tries to guess the DPI.
 *
 * @returns an guess for the dpi, or -1 if no guess could be made.
 */
double wayland_get_dpi_estimation(void);

#endif
