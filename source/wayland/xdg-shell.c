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

/** The Rofi Wayland Display log domain */
#define G_LOG_DOMAIN "Wayland"

#include <config.h>

#include <glib.h>
#include <wayland-client.h>

#include "rofi.h"
#include "settings.h"
#include "view.h"

#include "wayland-internal.h"
#include "xdg-decoration-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"

/**
 * Size the compositor asked for in the pending configure, in logical pixels.
 * Zero on an axis means the compositor leaves that one up to us, which is what
 * a floating window gets.
 */
static struct {
  int32_t width;
  int32_t height;
} wayland_xdg_pending = {0, 0};

static void wayland_xdg_toplevel_configure(G_GNUC_UNUSED void *data,
                                           G_GNUC_UNUSED struct xdg_toplevel *t,
                                           int32_t width, int32_t height,
                                           G_GNUC_UNUSED struct wl_array *s) {
  // Only recorded here; it takes effect on the xdg_surface configure, which
  // carries the serial we have to acknowledge.
  wayland_xdg_pending.width = width;
  wayland_xdg_pending.height = height;
}

static void
wayland_xdg_toplevel_close(G_GNUC_UNUSED void *data,
                           G_GNUC_UNUSED struct xdg_toplevel *toplevel) {
  g_debug("Compositor asked the toplevel to close");

  RofiViewState *state = rofi_view_get_active();
  if (state == NULL) {
    rofi_quit_main_loop();
    return;
  }
  rofi_view_cancel(state);
  rofi_view_maybe_update(state);
}

static const struct xdg_toplevel_listener wayland_xdg_toplevel_listener = {
    .configure = wayland_xdg_toplevel_configure,
    .close = wayland_xdg_toplevel_close,
};

static void wayland_xdg_surface_configure(G_GNUC_UNUSED void *data,
                                          struct xdg_surface *surface,
                                          uint32_t serial) {
  int32_t width = wayland_xdg_pending.width;
  int32_t height = wayland_xdg_pending.height;

  g_debug("xdg surface configure: %" PRIi32 "x%" PRIi32, width, height);

  // Acknowledge before attaching the buffer that answers this configure.
  xdg_surface_ack_configure(surface, serial);

  wayland_rofi_view_set_configured_size(width, height);

  // The first configure arrives while the display is still being set up, so
  // there is no view yet. It picks the size up when it computes its geometry.
  RofiViewState *state = rofi_view_get_active();
  if (state != NULL) {
    rofi_view_set_size(state, width > 0 ? width : -1, height > 0 ? height : -1);
  }
}

static const struct xdg_surface_listener wayland_xdg_surface_listener = {
    .configure = wayland_xdg_surface_configure,
};

static gboolean wayland_xdg_shell_create_surface(struct wl_output *wlo) {
  if (wlo != NULL) {
    g_debug("A normal window is placed by the compositor, ignoring the "
            "requested monitor");
  }

  wayland->xdg_surface =
      xdg_wm_base_get_xdg_surface(wayland->xdg_wm_base, wayland->surface);
  xdg_surface_add_listener(wayland->xdg_surface, &wayland_xdg_surface_listener,
                           NULL);

  wayland->xdg_toplevel = xdg_surface_get_toplevel(wayland->xdg_surface);
  xdg_toplevel_add_listener(wayland->xdg_toplevel,
                            &wayland_xdg_toplevel_listener, NULL);

  xdg_toplevel_set_app_id(wayland->xdg_toplevel, config.app_id);
  xdg_toplevel_set_title(wayland->xdg_toplevel, "rofi");

  // Rofi draws no decorations of its own, which is the same as what
  // -normal-window gets on X11.
  if (wayland->xdg_decoration_manager != NULL) {
    wayland->xdg_decoration =
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            wayland->xdg_decoration_manager, wayland->xdg_toplevel);
    zxdg_toplevel_decoration_v1_set_mode(
        wayland->xdg_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }

  return TRUE;
}

static void wayland_xdg_shell_destroy_surface(void) {
  if (wayland->xdg_decoration != NULL) {
    zxdg_toplevel_decoration_v1_destroy(wayland->xdg_decoration);
    wayland->xdg_decoration = NULL;
  }
  if (wayland->xdg_toplevel != NULL) {
    xdg_toplevel_destroy(wayland->xdg_toplevel);
    wayland->xdg_toplevel = NULL;
  }
  if (wayland->xdg_surface != NULL) {
    xdg_surface_destroy(wayland->xdg_surface);
    wayland->xdg_surface = NULL;
  }
  wayland_xdg_pending.width = 0;
  wayland_xdg_pending.height = 0;
}

static void wayland_xdg_shell_set_dimensions(int width, int height,
                                             G_GNUC_UNUSED int x_margin,
                                             G_GNUC_UNUSED int y_margin,
                                             G_GNUC_UNUSED int loc) {
  if (wayland->xdg_surface == NULL) {
    return;
  }

  // A toplevel has no anchors or margins; the compositor places it. Keep the
  // compositor's idea of the window in step with what we actually draw.
  if (width > 0 && height > 0) {
    xdg_surface_set_window_geometry(wayland->xdg_surface, 0, 0, width, height);
  }
}

static gboolean wayland_xdg_shell_get_output_dimensions(int *width,
                                                        int *height) {
  return wayland_display_get_output_size(width, height);
}

static void wayland_xdg_shell_set_fullscreen(void) {
  if (wayland->xdg_toplevel == NULL) {
    return;
  }
  xdg_toplevel_set_fullscreen(wayland->xdg_toplevel, NULL);
  wl_surface_commit(wayland->surface);
  wl_display_roundtrip(wayland->display);

  rofi_view_pool_refresh();
}

static void wayland_xdg_shell_set_title(const char *title) {
  if (wayland->xdg_toplevel == NULL || title == NULL) {
    return;
  }
  xdg_toplevel_set_title(wayland->xdg_toplevel, title);
}

const wayland_shell wayland_xdg_shell = {
    .name = "xdg shell",
    .captures_outside_clicks = FALSE,
    .create_surface = wayland_xdg_shell_create_surface,
    .destroy_surface = wayland_xdg_shell_destroy_surface,
    .set_dimensions = wayland_xdg_shell_set_dimensions,
    .get_output_dimensions = wayland_xdg_shell_get_output_dimensions,
    .set_fullscreen = wayland_xdg_shell_set_fullscreen,
    .set_title = wayland_xdg_shell_set_title,
};
