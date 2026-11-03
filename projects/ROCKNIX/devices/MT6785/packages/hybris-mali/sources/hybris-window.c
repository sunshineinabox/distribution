// SPDX-License-Identifier: Apache-2.0
// Native windows for the stock Mali blob: it renders into hybris-gralloc buffers, linear or AFBC,
// and each queued buffer goes to the compositor as linux-dmabuf without a copy. kbase fences no
// dma-buf, so the blob's fences go on the buffers. Headless windows only recycle buffers.
#define _GNU_SOURCE
#include <errno.h>
#include <linux/dma-buf.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <system/window.h>
#include <time.h>
#include <unistd.h>
#include <wayland-egl-backend.h>
#include "hybris-window.h"
#include "linux-dmabuf-v1-client-protocol.h"

#define NWIN 4			/* the fourth only once three are queued or on screen */
#define CONSUMER_USAGE 0x900	/* GPU texture + composer */
#define WIN_OF(p) ((struct hyb_window *)(p))	/* anw comes first */

struct win_buf {
	struct hyb_buffer *buf;
	struct wl_buffer *wl;
	int w, h, opaque, busy, dequeued;
	struct win_buf *next;		/* orphans: still shown, dropped once released */
};

struct hyb_window {
	struct ANativeWindow anw;
	struct hyb_window_config c;
	struct hyb_wl_format formats[HYB_NFORMATS];
	struct wl_event_queue *queue;		/* the window's own, like Mesa's */
	struct wl_surface *surf;		/* wrappers on queue */
	struct zwp_linux_dmabuf_v1 *dmabuf;
	struct wl_callback *frame;
	struct win_buf buf[NWIN];
	struct win_buf *orphans;
	uint32_t fourcc;
	int w, h, interval, sync, opaque, detached;
	int nbuf;			/* a swapchain's fixed image count, else NWIN */
	uint64_t frame_id;
	double cost;
	int frames;
	pthread_mutex_t mutex;
	atomic_int refs;			/* the creator's and the blob's */
};

static void dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *z, uint32_t format)
{
	struct hyb_wl_format *formats = data;
	const struct hyb_format *f = hyb_format(format);

	if (f)
		formats[f - hyb_formats].supported = 1;
}

static void dmabuf_modifier(void *data, struct zwp_linux_dmabuf_v1 *z, uint32_t format, uint32_t hi, uint32_t lo)
{
	struct hyb_wl_format *formats = data;
	const struct hyb_format *f = hyb_format(format);
	struct hyb_wl_format *e = f ? &formats[f - hyb_formats] : NULL;

	if (e && e->n < 8) {
		e->supported = 1;
		e->mods[e->n++] = (uint64_t)hi << 32 | lo;
	}
}
static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = { dmabuf_format, dmabuf_modifier };

void hyb_window_listen_dmabuf(struct zwp_linux_dmabuf_v1 *dmabuf, struct hyb_wl_format *formats)
{
	zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, formats);
}

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static uint32_t hal_fourcc(const struct hyb_window *w, int hal)
{
	for (size_t i = 0; i < HYB_NFORMATS; i++)
		if (hyb_formats[i].hal == hal && w->formats[i].supported)
			return hyb_formats[i].fourcc;
	return 0;
}

static void frame_done(void *data, struct wl_callback *cb, uint32_t t)
{
	wl_callback_destroy(cb);
	WIN_OF(data)->frame = NULL;
}
static const struct wl_callback_listener frame_listener = { frame_done };

static void buf_release(void *data, struct wl_buffer *wb)
{
	((struct win_buf *)data)->busy = 0;
}
static const struct wl_buffer_listener buf_listener = { buf_release };

static void drop_buffer(struct win_buf *b)
{
	if (b->wl)
		wl_buffer_destroy(b->wl);
	hyb_buffer_unref(b->buf);
	memset(b, 0, sizeof(*b));
}

/* The same buffer with its alpha ignored */
static uint32_t opaque_fourcc(uint32_t fourcc)
{
	return fourcc == HYB_ABGR8888 ? HYB_XBGR8888 : fourcc == HYB_ARGB8888 ? HYB_XRGB8888 : fourcc;
}

/* A buffer the compositor still shows, out of date or beyond a swapchain's count, leaves its
 * slot to a new one, as Android's BufferQueue lets SurfaceFlinger keep old buffers */
static void orphan(struct hyb_window *w, struct win_buf *b)
{
	struct win_buf *o = malloc(sizeof(*o));

	if (!o)
		return;		/* the slot waits for the release instead */
	*o = *b;
	wl_proxy_set_user_data((struct wl_proxy *)o->wl, o);
	o->next = w->orphans;
	w->orphans = o;
	memset(b, 0, sizeof(*b));
}

static void reap_orphans(struct hyb_window *w)
{
	for (struct win_buf **pp = &w->orphans, *o; (o = *pp);) {
		if (o->busy) {
			pp = &o->next;
			continue;
		}
		*pp = o->next;
		drop_buffer(o);
		free(o);
	}
}

static int stale(const struct hyb_window *w, const struct win_buf *b)
{
	return b->w != w->w || b->h != w->h || hyb_buffer_format(b->buf) != w->fourcc || b->opaque != w->opaque;
}

static int alloc_buffer(struct hyb_window *w, struct win_buf *b)
{
	uint32_t shown = w->opaque ? opaque_fourcc(w->fourcc) : w->fourcc;
	const struct hyb_format *f = hyb_format(w->fourcc), *fs = hyb_format(shown);
	const struct hyb_wl_format *e = fs ? &w->formats[fs - hyb_formats] : NULL;
	uint64_t mod = f && e ? hyb_pick_modifier(f, w->w, w->h, e->mods, e->n) : HYB_MOD_INVALID;
	struct zwp_linux_buffer_params_v1 *p;

	if (mod == HYB_MOD_INVALID || !(b->buf = hyb_buffer_alloc(w->w, w->h, w->fourcc, mod)))
		return -1;
	if (w->dmabuf) {
		p = zwp_linux_dmabuf_v1_create_params(w->dmabuf);
		zwp_linux_buffer_params_v1_add(p, hyb_buffer_fd(b->buf), 0, 0, hyb_buffer_pitch(b->buf),
					       mod >> 32, mod);
		b->wl = zwp_linux_buffer_params_v1_create_immed(p, w->w, w->h, shown, 0);
		zwp_linux_buffer_params_v1_destroy(p);
		wl_buffer_add_listener(b->wl, &buf_listener, b);
	}
	b->w = w->w;
	b->h = w->h;
	b->opaque = w->opaque;
	if (w->c.stats)
		fprintf(stderr, "hybris-window: %dx%d %.4s buffer, modifier %#llx\n", b->w, b->h,
			(const char *)&shown, (unsigned long long)mod);
	return 0;
}

/* What still uses the buffer, for the blob to wait on before rendering there again; -1 once idle */
static int release_fence(const struct hyb_window *w, struct hyb_buffer *buf)
{
	struct dma_buf_export_sync_file arg = { .flags = DMA_BUF_SYNC_WRITE, .fd = -1 };
	struct pollfd p = { .events = POLLIN };

	if (!w->sync || ioctl(hyb_buffer_fd(buf), DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &arg))
		return -1;
	p.fd = arg.fd;
	if (poll(&p, 1, 0) == 1) {
		close(arg.fd);
		return -1;
	}
	return arg.fd;
}

static void wait_fence(int fence)
{
	struct pollfd p = { .fd = fence, .events = POLLIN };

	if (fence < 0)
		return;
	while (poll(&p, 1, -1) < 0 && errno == EINTR)
		;
	close(fence);
}

/* The blob's fence goes on the buffer for the compositor and the display to wait on,
 * or the frame is finished here */
static void fence_buffer(const struct hyb_window *w, struct hyb_buffer *buf, int fence)
{
	struct dma_buf_import_sync_file arg = { .flags = DMA_BUF_SYNC_WRITE, .fd = fence };

	if (fence < 0)
		return;
	if (w->sync && !ioctl(hyb_buffer_fd(buf), DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &arg))
		close(fence);
	else
		wait_fence(fence);
}

/* Attach at the window's pending offset, which wl_surface takes as a request from v5 */
static void attach(struct hyb_window *w, struct wl_buffer *b)
{
	struct wl_egl_window *egl = w->c.egl;
	int dx = egl ? egl->dx : 0, dy = egl ? egl->dy : 0;

	if (wl_proxy_get_version((struct wl_proxy *)w->surf) >= WL_SURFACE_OFFSET_SINCE_VERSION) {
		if (dx || dy)
			wl_surface_offset(w->surf, dx, dy);
		wl_surface_attach(w->surf, b, 0, 0);
	} else {
		wl_surface_attach(w->surf, b, dx, dy);
	}
	if (egl) {
		egl->dx = 0;
		egl->dy = 0;
	}
}

static int win_dequeue(struct ANativeWindow *anw, struct ANativeWindowBuffer **out, int *fence)
{
	struct hyb_window *w = WIN_OF(anw);
	struct win_buf *b = NULL;
	int ret = 0;

	pthread_mutex_lock(&w->mutex);
	if (w->c.egl && w->c.egl->width > 0 && w->c.egl->height > 0) {
		w->w = w->c.egl->width;
		w->h = w->c.egl->height;
	}
	for (int i = 0; i < NWIN; i++) {
		struct win_buf *o = &w->buf[i];

		if (o->buf && o->busy && !o->dequeued && (i >= w->nbuf || stale(w, o)))
			orphan(w, o);
	}
	reap_orphans(w);
	while (!b) {
		for (int i = 0; i < w->nbuf && !b; i++)
			if (!w->buf[i].busy && !w->buf[i].dequeued)
				b = &w->buf[i];
		if (!b && (!w->queue || wl_display_dispatch_queue(w->c.display, w->queue) < 0))
			break;
	}
	if (b && b->buf && stale(w, b))
		drop_buffer(b);
	if (!b)
		ret = -EIO;
	else if (!b->buf && alloc_buffer(w, b) < 0)
		ret = -ENOMEM;
	else {
		b->dequeued = 1;
		*out = hyb_buffer_native(b->buf);
		*fence = release_fence(w, b->buf);
	}
	pthread_mutex_unlock(&w->mutex);
	return ret;
}

static struct win_buf *find_buffer(struct hyb_window *w, struct ANativeWindowBuffer *buffer)
{
	for (int i = 0; i < NWIN; i++)
		if (w->buf[i].buf && hyb_buffer_native(w->buf[i].buf) == buffer)
			return &w->buf[i];
	return NULL;
}

static int win_queue(struct ANativeWindow *anw, struct ANativeWindowBuffer *buffer, int fence)
{
	struct hyb_window *w = WIN_OF(anw);
	double t0 = now_ms();
	struct win_buf *b;

	pthread_mutex_lock(&w->mutex);
	b = find_buffer(w, buffer);
	if (!b) {
		pthread_mutex_unlock(&w->mutex);
		if (fence >= 0)
			close(fence);
		return -EINVAL;
	}
	fence_buffer(w, b->buf, fence);
	b->dequeued = 0;
	w->frame_id++;
	if (w->c.stats) {
		w->cost += now_ms() - t0;
		if (++w->frames == 120) {
			fprintf(stderr, "hybris-window: %.2f ms per frame waiting for the GPU\n", w->cost / w->frames);
			w->cost = 0;
			w->frames = 0;
		}
	}
	if (w->surf && b->wl && !w->detached) {
		while (w->frame && w->interval > 0)
			if (wl_display_dispatch_queue(w->c.display, w->queue) < 0)
				break;
		if (w->interval > 0) {
			w->frame = wl_surface_frame(w->surf);
			wl_callback_add_listener(w->frame, &frame_listener, w);
		}
		b->busy = 1;
		attach(w, b->wl);
		if (w->c.egl) {
			w->c.egl->attached_width = b->w;
			w->c.egl->attached_height = b->h;
		}
		if (wl_proxy_get_version((struct wl_proxy *)w->surf) >= 4)
			wl_surface_damage_buffer(w->surf, 0, 0, INT32_MAX, INT32_MAX);
		else
			wl_surface_damage(w->surf, 0, 0, INT32_MAX, INT32_MAX);
		wl_surface_commit(w->surf);
		wl_display_flush(w->c.display);
	}
	pthread_mutex_unlock(&w->mutex);
	return 0;
}

static int win_cancel(struct ANativeWindow *anw, struct ANativeWindowBuffer *buffer, int fence)
{
	struct hyb_window *w = WIN_OF(anw);
	struct win_buf *b;

	pthread_mutex_lock(&w->mutex);
	b = find_buffer(w, buffer);
	if (b) {
		fence_buffer(w, b->buf, fence);	/* what the blob did there so far */
		b->dequeued = 0;
	} else if (fence >= 0) {
		close(fence);
	}
	pthread_mutex_unlock(&w->mutex);
	return 0;
}

static int win_dequeue_old(struct ANativeWindow *anw, struct ANativeWindowBuffer **out)
{
	int fence, ret = win_dequeue(anw, out, &fence);

	if (!ret)
		wait_fence(fence);
	return ret;
}

static int win_lock_old(struct ANativeWindow *anw, struct ANativeWindowBuffer *buffer)
{
	return 0;
}

static int win_queue_old(struct ANativeWindow *anw, struct ANativeWindowBuffer *buffer)
{
	return win_queue(anw, buffer, -1);
}

static int win_cancel_old(struct ANativeWindow *anw, struct ANativeWindowBuffer *buffer)
{
	return win_cancel(anw, buffer, -1);
}

static int win_interval(struct ANativeWindow *anw, int interval)
{
	WIN_OF(anw)->interval = interval > 0 ? interval : 0;
	return 0;
}

static int win_query(const struct ANativeWindow *anw, int what, int *value)
{
	struct hyb_window *w = WIN_OF(anw);
	const struct hyb_format *f;
	int ret = 0;

	pthread_mutex_lock(&w->mutex);
	f = hyb_format(w->fourcc);
	switch (what) {
	case NATIVE_WINDOW_WIDTH:
		*value = w->c.egl && w->c.egl->width > 0 ? w->c.egl->width : w->w;
		break;
	case NATIVE_WINDOW_HEIGHT:
		*value = w->c.egl && w->c.egl->height > 0 ? w->c.egl->height : w->h;
		break;
	/* without a wl_egl_window, any: a Vulkan swapchain sets the size, as on Wayland */
	case NATIVE_WINDOW_DEFAULT_WIDTH:
		*value = !w->c.egl ? -1 : w->c.egl->width > 0 ? w->c.egl->width : w->w;
		break;
	case NATIVE_WINDOW_DEFAULT_HEIGHT:
		*value = !w->c.egl ? -1 : w->c.egl->height > 0 ? w->c.egl->height : w->h;
		break;
	case NATIVE_WINDOW_FORMAT:
		*value = f ? f->hal : 0;
		break;
	case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
	case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER:
	case NATIVE_WINDOW_LAYER_COUNT:
		*value = 1;
		break;
	case NATIVE_WINDOW_CONCRETE_TYPE:
		*value = NATIVE_WINDOW_SURFACE;
		break;
	case NATIVE_WINDOW_CONSUMER_USAGE_BITS:
		*value = CONSUMER_USAGE;
		break;
	case NATIVE_WINDOW_IS_VALID:
		*value = !w->detached;
		break;
	case NATIVE_WINDOW_MAX_BUFFER_COUNT:
		*value = NWIN;
		break;
	case NATIVE_WINDOW_TRANSFORM_HINT:
	case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
	case NATIVE_WINDOW_STICKY_TRANSFORM:
	case NATIVE_WINDOW_DEFAULT_DATASPACE:
	case NATIVE_WINDOW_BUFFER_AGE:		/* 0: apps redraw whole frames */
	case NATIVE_WINDOW_LAST_DEQUEUE_DURATION:
	case NATIVE_WINDOW_LAST_QUEUE_DURATION:
	case NATIVE_WINDOW_FRAME_TIMESTAMPS_SUPPORTS_PRESENT:
	case NATIVE_WINDOW_CONSUMER_IS_PROTECTED:
	case NATIVE_WINDOW_DATASPACE:
		*value = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	pthread_mutex_unlock(&w->mutex);
	return ret;
}

static int win_perform(struct ANativeWindow *anw, int op, ...)
{
	struct hyb_window *w = WIN_OF(anw);
	uint32_t fourcc;
	va_list ap;
	int ret = 0, bw, bh;

	va_start(ap, op);
	switch (op) {
	case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
		bw = va_arg(ap, int);
		pthread_mutex_lock(&w->mutex);
		fourcc = hal_fourcc(w, bw);
		if (fourcc)
			w->fourcc = fourcc;
		else if (bw)
			ret = -EINVAL;	/* the blob would render a format the buffers are not */
		pthread_mutex_unlock(&w->mutex);
		break;
	case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
	case NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS:
		bw = va_arg(ap, int);
		bh = va_arg(ap, int);
		/* a swapchain's image size; a wl_egl_window has its own */
		pthread_mutex_lock(&w->mutex);
		if (!w->c.egl && hyb_size_ok(bw, bh)) {
			w->w = bw;
			w->h = bh;
		}
		pthread_mutex_unlock(&w->mutex);
		break;
	case NATIVE_WINDOW_SET_BUFFER_COUNT:
		/* a swapchain knows its buffers by their handles: no others from here on */
		bw = va_arg(ap, int);
		pthread_mutex_lock(&w->mutex);
		w->nbuf = bw > 0 && bw < NWIN ? bw : NWIN;
		for (int i = w->nbuf; i < NWIN; i++)
			if (!w->buf[i].dequeued && !w->buf[i].busy)
				drop_buffer(&w->buf[i]);
		pthread_mutex_unlock(&w->mutex);
		break;
	case NATIVE_WINDOW_SET_SHARED_BUFFER_MODE:
	case NATIVE_WINDOW_SET_AUTO_REFRESH:
		ret = va_arg(ap, int) ? -EINVAL : 0;
		break;
	case NATIVE_WINDOW_GET_REFRESH_CYCLE_DURATION:
		*va_arg(ap, int64_t *) = 16666667;
		break;
	case NATIVE_WINDOW_GET_NEXT_FRAME_ID:
		*va_arg(ap, uint64_t *) = w->frame_id + 1;
		break;
	case NATIVE_WINDOW_GET_WIDE_COLOR_SUPPORT:
	case NATIVE_WINDOW_GET_HDR_SUPPORT:
		*va_arg(ap, bool *) = false;
		break;
	case NATIVE_WINDOW_GET_CONSUMER_USAGE64:
		*va_arg(ap, uint64_t *) = CONSUMER_USAGE;
		break;
	case NATIVE_WINDOW_GET_LAST_DEQUEUE_START:
	case NATIVE_WINDOW_GET_LAST_DEQUEUE_DURATION:
	case NATIVE_WINDOW_GET_LAST_QUEUE_DURATION:
		*va_arg(ap, int64_t *) = 0;
		break;
	case NATIVE_WINDOW_SET_USAGE:
	case NATIVE_WINDOW_CONNECT:
	case NATIVE_WINDOW_DISCONNECT:
	case NATIVE_WINDOW_SET_CROP:
	case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
	case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
	case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
	case NATIVE_WINDOW_SET_SCALING_MODE:
	case NATIVE_WINDOW_API_CONNECT:
	case NATIVE_WINDOW_API_DISCONNECT:
	case NATIVE_WINDOW_SET_BUFFERS_STICKY_TRANSFORM:
	case NATIVE_WINDOW_SET_BUFFERS_DATASPACE:
	case NATIVE_WINDOW_SET_SURFACE_DAMAGE:
	case NATIVE_WINDOW_ENABLE_FRAME_TIMESTAMPS:
	case NATIVE_WINDOW_SET_USAGE64:
	case NATIVE_WINDOW_SET_BUFFERS_SMPTE2086_METADATA:
	case NATIVE_WINDOW_SET_BUFFERS_CTA861_3_METADATA:
	case NATIVE_WINDOW_SET_BUFFERS_HDR10_PLUS_METADATA:
	case NATIVE_WINDOW_SET_AUTO_PREROTATION:
	case NATIVE_WINDOW_SET_DEQUEUE_TIMEOUT:
	case NATIVE_WINDOW_SET_FRAME_RATE:
	case NATIVE_WINDOW_ALLOCATE_BUFFERS:
		break;		/* nothing to set up: buffers follow the window */
	default:
		ret = -ENOENT;
		break;
	}
	va_end(ap);
	return ret;
}

static void free_window(struct hyb_window *w)
{
	for (struct win_buf *o; (o = w->orphans);) {
		w->orphans = o->next;
		drop_buffer(o);
		free(o);
	}
	for (int i = 0; i < NWIN; i++)
		drop_buffer(&w->buf[i]);
	if (w->frame)
		wl_callback_destroy(w->frame);
	if (w->queue) {
		wl_proxy_wrapper_destroy(w->surf);
		if (w->dmabuf)
			wl_proxy_wrapper_destroy(w->dmabuf);
		wl_event_queue_destroy(w->queue);
	}
	pthread_mutex_destroy(&w->mutex);
	free(w);
}

static void win_ref(struct android_native_base_t *base)
{
	atomic_fetch_add(&WIN_OF(base)->refs, 1);
}

static void win_unref(struct android_native_base_t *base)
{
	if (atomic_fetch_sub(&WIN_OF(base)->refs, 1) == 1)
		free_window(WIN_OF(base));
}

struct hyb_window *hyb_window_create(const struct hyb_window_config *c)
{
	struct hyb_window *w = calloc(1, sizeof(*w));
	uint32_t fourcc;

	if (!w)
		return NULL;
	w->c = *c;
	w->c.formats = NULL;
	if (c->formats) {
		memcpy(w->formats, c->formats, sizeof(w->formats));
	} else {
		/* headless: whatever the blob renders */
		for (size_t i = 0; i < HYB_NFORMATS; i++) {
			struct hyb_wl_format *e = &w->formats[i];

			e->supported = 1;
			e->mods[e->n++] = HYB_MOD_LINEAR;
			if (hyb_formats[i].afbc)
				e->mods[e->n++] = HYB_MOD_AFBC;
			if (hyb_formats[i].afbc == 2)
				e->mods[e->n++] = HYB_MOD_AFBC_YTR;
		}
	}
	fourcc = hal_fourcc(w, c->hal_format);
	w->fourcc = fourcc ? fourcc : HYB_XBGR8888;
	w->w = c->egl && c->egl->width > 0 ? c->egl->width : 1;
	w->h = c->egl && c->egl->height > 0 ? c->egl->height : 1;
	w->interval = 1;
	w->nbuf = NWIN;
	/* headless, the only consumer is the blob itself */
	w->sync = c->sync_files || !c->surface;
	atomic_init(&w->refs, 1);
	pthread_mutex_init(&w->mutex, NULL);
	if (c->surface) {
		w->queue = wl_display_create_queue(c->display);
		w->surf = wl_proxy_create_wrapper(c->surface);
		wl_proxy_set_queue((struct wl_proxy *)w->surf, w->queue);
		if (c->dmabuf) {
			w->dmabuf = wl_proxy_create_wrapper(c->dmabuf);
			wl_proxy_set_queue((struct wl_proxy *)w->dmabuf, w->queue);
		}
	}
	memcpy(&w->anw, &(struct ANativeWindow){
		.common = {
			.magic = ANDROID_NATIVE_WINDOW_MAGIC,
			.version = sizeof(struct ANativeWindow),
			.incRef = win_ref,
			.decRef = win_unref,
		},
		.minSwapInterval = 0,
		.maxSwapInterval = 1,
		.setSwapInterval = win_interval,
		.dequeueBuffer_DEPRECATED = win_dequeue_old,
		.lockBuffer_DEPRECATED = win_lock_old,
		.queueBuffer_DEPRECATED = win_queue_old,
		.query = win_query,
		.perform = win_perform,
		.cancelBuffer_DEPRECATED = win_cancel_old,
		.dequeueBuffer = win_dequeue,
		.queueBuffer = win_queue,
		.cancelBuffer = win_cancel,
	}, sizeof(w->anw));
	return w;
}

struct ANativeWindow *hyb_window_native(struct hyb_window *w)
{
	return &w->anw;
}

void hyb_window_set_opaque(struct hyb_window *w, int opaque)
{
	pthread_mutex_lock(&w->mutex);
	w->opaque = opaque;
	pthread_mutex_unlock(&w->mutex);
}

void hyb_window_detach(struct hyb_window *w)
{
	pthread_mutex_lock(&w->mutex);
	w->detached = 1;
	w->c.egl = NULL;
	if (w->frame) {
		wl_callback_destroy(w->frame);
		w->frame = NULL;
	}
	pthread_mutex_unlock(&w->mutex);
}

void hyb_window_unref(struct hyb_window *w)
{
	win_unref(&w->anw.common);
}
