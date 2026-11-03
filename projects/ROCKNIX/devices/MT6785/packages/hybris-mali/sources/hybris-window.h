// SPDX-License-Identifier: Apache-2.0
// ANativeWindow for the stock Mali blob, shared by the EGL and Vulkan shims. The blob renders
// into hybris-gralloc buffers, linear or AFBC, and each frame goes out as a linux-dmabuf buffer.
#ifndef HYBRIS_WINDOW_H
#define HYBRIS_WINDOW_H

#include <stdint.h>
#include <wayland-client.h>
#include "hybris-gralloc.h"

struct ANativeWindow;
struct wl_egl_window;
struct zwp_linux_dmabuf_v1;
struct hyb_window;

/* What the compositor imports, per hyb_formats entry */
struct hyb_wl_format {
	int supported, n;
	uint64_t mods[8];
};

struct hyb_window_config {
	struct wl_display *display;
	struct wl_surface *surface;		/* NULL: headless, frames are only recycled */
	struct zwp_linux_dmabuf_v1 *dmabuf;
	const struct hyb_wl_format *formats;	/* indexed like hyb_formats, copied */
	struct wl_egl_window *egl;		/* size and offset from it, else from the swapchain */
	int hal_format;				/* the buffers' HAL_PIXEL_FORMAT_*, 0 for RGBX_8888 */
	int sync_files;				/* the compositor waits on dma-buf fences */
	int stats;
};

/* Fill formats from the compositor's zwp_linux_dmabuf_v1 (version 3) events */
void hyb_window_listen_dmabuf(struct zwp_linux_dmabuf_v1 *dmabuf, struct hyb_wl_format *formats);

struct hyb_window *hyb_window_create(const struct hyb_window_config *c);
struct ANativeWindow *hyb_window_native(struct hyb_window *w);
/* An opaque swapchain's alpha is not the compositor's to blend */
void hyb_window_set_opaque(struct hyb_window *w, int opaque);
/* Present nothing more and forget the wl_egl_window, as its owner is done with them */
void hyb_window_detach(struct hyb_window *w);
/* Drop the creator's reference: the window goes with the blob's last one */
void hyb_window_unref(struct hyb_window *w);

#endif
