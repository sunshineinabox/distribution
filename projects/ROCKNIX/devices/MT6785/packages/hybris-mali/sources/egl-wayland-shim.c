// SPDX-License-Identifier: Apache-2.0
// libEGL.so.1 shim over libhybris: Wayland window surfaces are pbuffers, and
// eglSwapBuffers hands each frame to the compositor as a wl_shm buffer, so
// unmodified Wayland EGL apps run on an Android EGL driver under a plain
// compositor. With /dev/udmabuf the GPU blits into the shm memory directly;
// otherwise the frame is read back with glReadPixels.
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/udmabuf.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl-backend.h>

#define EXPORT __attribute__((visibility("default")))
#ifndef REAL_EGL
#define REAL_EGL "/usr/lib/libhybris/libEGL.so.1"
#endif
#define NBUF 3
#define GL_BGRA_EXT 0x80E1
#define DRM_FORMAT_XBGR8888 0x34324258	/* 'XB24': R, G, B, X bytes = GL_RGBA order */

/* EGL entry points forwarded unchanged: ret, name, params, args */
#define PASSTHROUGH(X) \
	X(EGLBoolean, eglBindAPI, (EGLenum api), (api)) \
	X(EGLBoolean, eglBindTexImage, (EGLDisplay d, EGLSurface s, EGLint b), (d, map(s), b)) \
	X(EGLint, eglClientWaitSync, (EGLDisplay d, EGLSync s, EGLint f, EGLTime t), (d, s, f, t)) \
	X(EGLBoolean, eglCopyBuffers, (EGLDisplay d, EGLSurface s, EGLNativePixmapType p), (d, map(s), p)) \
	X(EGLContext, eglCreateContext, (EGLDisplay d, EGLConfig c, EGLContext s, const EGLint *a), (d, c, s, a)) \
	X(EGLImage, eglCreateImage, (EGLDisplay d, EGLContext c, EGLenum t, EGLClientBuffer b, const EGLAttrib *a), (d, c, t, b, a)) \
	X(EGLSurface, eglCreatePbufferFromClientBuffer, (EGLDisplay d, EGLenum t, EGLClientBuffer b, EGLConfig c, const EGLint *a), (d, t, b, c, a)) \
	X(EGLSurface, eglCreatePbufferSurface, (EGLDisplay d, EGLConfig c, const EGLint *a), (d, c, a)) \
	X(EGLSurface, eglCreatePixmapSurface, (EGLDisplay d, EGLConfig c, EGLNativePixmapType p, const EGLint *a), (d, c, p, a)) \
	X(EGLSurface, eglCreatePlatformPixmapSurface, (EGLDisplay d, EGLConfig c, void *p, const EGLAttrib *a), (d, c, p, a)) \
	X(EGLSync, eglCreateSync, (EGLDisplay d, EGLenum t, const EGLAttrib *a), (d, t, a)) \
	X(EGLBoolean, eglDestroyContext, (EGLDisplay d, EGLContext c), (d, c)) \
	X(EGLBoolean, eglDestroyImage, (EGLDisplay d, EGLImage i), (d, i)) \
	X(EGLBoolean, eglDestroySync, (EGLDisplay d, EGLSync s), (d, s)) \
	X(EGLBoolean, eglGetConfigs, (EGLDisplay d, EGLConfig *c, EGLint n, EGLint *r), (d, c, n, r)) \
	X(EGLContext, eglGetCurrentContext, (void), ()) \
	X(EGLDisplay, eglGetCurrentDisplay, (void), ()) \
	X(EGLBoolean, eglGetSyncAttrib, (EGLDisplay d, EGLSync s, EGLint a, EGLAttrib *v), (d, s, a, v)) \
	X(EGLBoolean, eglInitialize, (EGLDisplay d, EGLint *ma, EGLint *mi), (d, ma, mi)) \
	X(EGLenum, eglQueryAPI, (void), ()) \
	X(EGLBoolean, eglQueryContext, (EGLDisplay d, EGLContext c, EGLint a, EGLint *v), (d, c, a, v)) \
	X(EGLBoolean, eglReleaseTexImage, (EGLDisplay d, EGLSurface s, EGLint b), (d, map(s), b)) \
	X(EGLBoolean, eglReleaseThread, (void), ()) \
	X(EGLBoolean, eglSurfaceAttrib, (EGLDisplay d, EGLSurface s, EGLint a, EGLint v), (d, map(s), a, v)) \
	X(EGLBoolean, eglTerminate, (EGLDisplay d), (d)) \
	X(EGLBoolean, eglWaitClient, (void), ()) \
	X(EGLBoolean, eglWaitGL, (void), ()) \
	X(EGLBoolean, eglWaitNative, (EGLint e), (e)) \
	X(EGLBoolean, eglWaitSync, (EGLDisplay d, EGLSync s, EGLint f), (d, s, f))

/* entry points the shim wraps; only the real pointers are generated */
#define WRAPPED(X) \
	X(EGLBoolean, eglChooseConfig, (EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint n, EGLint *r)) \
	X(EGLSurface, eglCreateWindowSurface, (EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)) \
	X(EGLBoolean, eglDestroySurface, (EGLDisplay d, EGLSurface s)) \
	X(EGLBoolean, eglGetConfigAttrib, (EGLDisplay d, EGLConfig c, EGLint a, EGLint *v)) \
	X(EGLSurface, eglGetCurrentSurface, (EGLint w)) \
	X(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType n)) \
	X(EGLint, eglGetError, (void)) \
	X(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char *n)) \
	X(EGLBoolean, eglMakeCurrent, (EGLDisplay d, EGLSurface w, EGLSurface r, EGLContext c)) \
	X(const char *, eglQueryString, (EGLDisplay d, EGLint n)) \
	X(EGLBoolean, eglQuerySurface, (EGLDisplay d, EGLSurface s, EGLint a, EGLint *v)) \
	X(EGLBoolean, eglSwapBuffers, (EGLDisplay d, EGLSurface s)) \
	X(EGLBoolean, eglSwapInterval, (EGLDisplay d, EGLint i))

#define DECL_PT(ret, name, params, args) static ret (*real_##name) params;
#define DECL_W(ret, name, params) static ret (*real_##name) params;
PASSTHROUGH(DECL_PT)
WRAPPED(DECL_W)

static PFNEGLCREATEIMAGEKHRPROC create_image;
static PFNEGLDESTROYIMAGEKHRPROC destroy_image;

static struct {
	void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
	void (*GetIntegerv)(GLenum, GLint *);
	void (*BindFramebuffer)(GLenum, GLuint);
	void (*BindBuffer)(GLenum, GLuint);
	void (*PixelStorei)(GLenum, GLint);
	void (*GenFramebuffers)(GLsizei, GLuint *);
	void (*DeleteFramebuffers)(GLsizei, const GLuint *);
	void (*GenRenderbuffers)(GLsizei, GLuint *);
	void (*DeleteRenderbuffers)(GLsizei, const GLuint *);
	void (*BindRenderbuffer)(GLenum, GLuint);
	void (*FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
	GLenum (*CheckFramebufferStatus)(GLenum);
	void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
	void (*Finish)(void);
	GLboolean (*IsEnabled)(GLenum);
	void (*Enable)(GLenum);
	void (*Disable)(GLenum);
	void (*EGLImageTargetRenderbufferStorageOES)(GLenum, GLeglImageOES);
	const GLubyte *(*GetString)(GLenum);
} gl;

struct shm_buf {
	struct wl_buffer *wl;
	void *px;
	size_t size;
	int busy;
	int dmafd;		/* udmabuf over the shm memfd, or -1 */
	EGLImageKHR img;
	EGLContext ctx;		/* context owning rb/fb */
	GLuint rb, fb;
};

struct shim_surf {
	EGLDisplay dpy;
	EGLConfig cfg;
	EGLSurface real;
	struct wl_egl_window *win;
	struct wl_surface *surf;	/* wrapper on the shim's event queue */
	int w, h, interval, transform_set, zero_copy;
	struct shm_buf buf[NBUF];
	struct wl_callback *frame;
	double cost;
	int frames;
	struct shim_surf *next;
};

static struct {
	struct wl_display *wl;
	struct wl_event_queue *queue;
	struct wl_display *wrapper;
	struct wl_registry *registry;
	struct wl_shm *shm;
	int xbgr;
	int udmabuf;		/* /dev/udmabuf fd, -1 if unavailable */
} wl = { .udmabuf = -1 };

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct shim_surf *surfaces;
static __thread EGLint shim_error;
static int stats, force_readback;

static void load(void)
{
	const char *path = getenv("HYBRIS_SHIM_REAL_EGL");
	const char *copy = getenv("HYBRIS_SHIM_COPY");
	void *h = dlopen(path ? path : REAL_EGL, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	void *g = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);

	stats = getenv("HYBRIS_SHIM_STATS") != NULL;
	force_readback = copy && !strcmp(copy, "readback");
	if (!h) {
		fprintf(stderr, "egl-wayland-shim: %s\n", dlerror());
		return;
	}
#define LOAD(ret, name, ...) real_##name = (__typeof__(real_##name))dlsym(h, #name);
	PASSTHROUGH(LOAD)
	WRAPPED(LOAD)
	create_image = (PFNEGLCREATEIMAGEKHRPROC)real_eglGetProcAddress("eglCreateImageKHR");
	destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)real_eglGetProcAddress("eglDestroyImageKHR");
	if (g) {
#define GL(name) gl.name = dlsym(g, "gl" #name)
		GL(ReadPixels);
		GL(GetIntegerv);
		GL(BindFramebuffer);
		GL(BindBuffer);
		GL(PixelStorei);
		GL(GenFramebuffers);
		GL(DeleteFramebuffers);
		GL(GenRenderbuffers);
		GL(DeleteRenderbuffers);
		GL(BindRenderbuffer);
		GL(FramebufferRenderbuffer);
		GL(CheckFramebufferStatus);
		GL(BlitFramebuffer);
		GL(Finish);
		GL(IsEnabled);
		GL(Enable);
		GL(Disable);
		GL(EGLImageTargetRenderbufferStorageOES);
		GL(GetString);
	}
	if (!force_readback)
		wl.udmabuf = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
}

static void init(void)
{
	pthread_once(&once, load);
}

static double now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static struct shim_surf *lookup(EGLSurface s)
{
	struct shim_surf *p;

	pthread_mutex_lock(&lock);
	for (p = surfaces; p && (EGLSurface)p != s; p = p->next)
		;
	pthread_mutex_unlock(&lock);
	return p;
}

static EGLSurface map(EGLSurface s)
{
	struct shim_surf *p = lookup(s);
	return p ? p->real : s;
}

#define DEF_PT(ret, name, params, args) \
	EXPORT ret name params { init(); return real_##name ? real_##name args : (ret)0; }
PASSTHROUGH(DEF_PT)

/* Wayland connection on a private queue, like Mesa's platform_wayland */

static void shm_format(void *d, struct wl_shm *s, uint32_t f)
{
	if (f == WL_SHM_FORMAT_XBGR8888)
		wl.xbgr = 1;
}
static const struct wl_shm_listener shm_listener = { shm_format };

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver)
{
	if (!strcmp(iface, wl_shm_interface.name) && !wl.shm) {
		wl.shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
		wl_shm_add_listener(wl.shm, &shm_listener, NULL);
	}
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static int is_wl_display(void *native)
{
	return native && *(const struct wl_interface **)native == &wl_display_interface;
}

static void bind_display(struct wl_display *dpy)
{
	if (wl.wl == dpy)
		return;
	wl.wl = dpy;
	wl.queue = wl_display_create_queue(dpy);
	wl.wrapper = wl_proxy_create_wrapper(dpy);
	wl_proxy_set_queue((struct wl_proxy *)wl.wrapper, wl.queue);
	wl.registry = wl_display_get_registry(wl.wrapper);
	wl_registry_add_listener(wl.registry, &reg_listener, NULL);
	wl_display_roundtrip_queue(dpy, wl.queue);
	wl_display_roundtrip_queue(dpy, wl.queue);
}

EXPORT EGLDisplay eglGetDisplay(EGLNativeDisplayType native)
{
	init();
	if (is_wl_display((void *)native)) {
		bind_display((struct wl_display *)native);
		native = EGL_DEFAULT_DISPLAY;
	}
	return real_eglGetDisplay(native);
}

static EGLDisplay platform_display(EGLenum platform, void *native)
{
	init();
	if (platform == EGL_PLATFORM_WAYLAND_KHR && native)
		bind_display(native);
	else if (platform != EGL_PLATFORM_WAYLAND_KHR && platform != 0x31DD /* SURFACELESS_MESA */) {
		shim_error = EGL_BAD_PARAMETER;
		return EGL_NO_DISPLAY;
	}
	return real_eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EXPORT EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native, const EGLAttrib *a)
{
	return platform_display(platform, native);
}

static EGLDisplay get_platform_display_ext(EGLenum platform, void *native, const EGLint *a)
{
	return platform_display(platform, native);
}

EXPORT const char *eglQueryString(EGLDisplay d, EGLint name)
{
	static char client[1024];
	const char *s;

	init();
	s = real_eglQueryString(d, name);
	if (d != EGL_NO_DISPLAY || name != EGL_EXTENSIONS)
		return s;
	if (!client[0])
		snprintf(client, sizeof(client), "%s%sEGL_EXT_platform_base EGL_KHR_platform_wayland "
			 "EGL_EXT_platform_wayland EGL_MESA_platform_surfaceless",
			 s ? s : "", s && *s ? " " : "");
	return client;
}

EXPORT EGLint eglGetError(void)
{
	EGLint e = shim_error;

	init();
	if (e) {
		shim_error = 0;
		real_eglGetError();
		return e;
	}
	return real_eglGetError();
}

/* Window configs become pbuffer configs */
EXPORT EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *attr, EGLConfig *c, EGLint n, EGLint *r)
{
	EGLint a[128];
	int i = 0, have_type = 0;

	init();
	for (; attr && attr[0] != EGL_NONE && i < 124; attr += 2) {
		a[i++] = attr[0];
		a[i] = attr[1];
		if (attr[0] == EGL_SURFACE_TYPE) {
			have_type = 1;
			if (a[i] != EGL_DONT_CARE && (a[i] & EGL_WINDOW_BIT))
				a[i] = (a[i] & ~EGL_WINDOW_BIT) | EGL_PBUFFER_BIT;
		}
		i++;
	}
	if (!have_type) {
		a[i++] = EGL_SURFACE_TYPE;
		a[i++] = EGL_PBUFFER_BIT;
	}
	a[i] = EGL_NONE;
	return real_eglChooseConfig(d, a, c, n, r);
}

EXPORT EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v)
{
	EGLBoolean ok;

	init();
	ok = real_eglGetConfigAttrib(d, c, a, v);
	if (ok && a == EGL_SURFACE_TYPE && (*v & EGL_PBUFFER_BIT))
		*v |= EGL_WINDOW_BIT;
	return ok;
}

static void free_buffers(struct shim_surf *s)
{
	EGLContext cur = real_eglGetCurrentContext();
	int i;

	for (i = 0; i < NBUF; i++) {
		struct shm_buf *b = &s->buf[i];

		if (b->fb && b->ctx == cur) {
			gl.DeleteFramebuffers(1, &b->fb);
			gl.DeleteRenderbuffers(1, &b->rb);
		}
		if (b->img)
			destroy_image(s->dpy, b->img);
		if (b->dmafd >= 0)
			close(b->dmafd);
		if (b->wl)
			wl_buffer_destroy(b->wl);
		if (b->px)
			munmap(b->px, b->size);
		memset(b, 0, sizeof(*b));
		b->dmafd = -1;
	}
}

static void buf_release(void *d, struct wl_buffer *b)
{
	((struct shm_buf *)d)->busy = 0;
}
static const struct wl_buffer_listener buf_listener = { buf_release };

/* Import the shm memfd as an EGLImage the GPU can render into */
static int import_buffer(struct shim_surf *s, struct shm_buf *b, int memfd)
{
	struct udmabuf_create c = { .memfd = memfd, .flags = UDMABUF_FLAGS_CLOEXEC, .size = b->size };

	b->dmafd = ioctl(wl.udmabuf, UDMABUF_CREATE, &c);
	if (b->dmafd < 0)
		return -1;
	EGLint a[] = {
		EGL_WIDTH, s->w, EGL_HEIGHT, s->h,
		EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XBGR8888,
		EGL_DMA_BUF_PLANE0_FD_EXT, b->dmafd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, s->w * 4,
		EGL_NONE
	};
	b->img = create_image(s->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, a);
	return b->img == EGL_NO_IMAGE_KHR ? -1 : 0;
}

static int alloc_buffers(struct shim_surf *s)
{
	size_t size = ((size_t)s->w * s->h * 4 + 4095) & ~(size_t)4095;
	int i;

	for (i = 0; i < NBUF; i++) {
		struct shm_buf *b = &s->buf[i];
		int fd = memfd_create("egl-wayland-shim", MFD_CLOEXEC | MFD_ALLOW_SEALING);

		b->dmafd = -1;
		if (fd < 0 || ftruncate(fd, size) < 0)
			return -1;
		/* udmabuf needs the memfd unable to shrink */
		fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
		b->size = size;
		b->px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		struct wl_shm_pool *pool = wl_shm_create_pool(wl.shm, fd, size);
		b->wl = wl_shm_pool_create_buffer(pool, 0, s->w, s->h, s->w * 4,
			wl.xbgr ? WL_SHM_FORMAT_XBGR8888 : WL_SHM_FORMAT_XRGB8888);
		wl_buffer_add_listener(b->wl, &buf_listener, b);
		wl_shm_pool_destroy(pool);
		if (s->zero_copy && import_buffer(s, b, fd) < 0) {
			fprintf(stderr, "egl-wayland-shim: dma-buf import failed (0x%x), reading back\n",
				real_eglGetError());
			s->zero_copy = 0;
		}
		close(fd);
		if (b->px == MAP_FAILED)
			return -1;
	}
	return 0;
}

static void win_resize(struct wl_egl_window *w, void *priv) {}

static void win_destroy(void *priv)
{
	struct shim_surf *s = priv;
	if (s)
		s->win = NULL;
}

static int can_zero_copy(EGLDisplay d)
{
	const char *ext = real_eglQueryString(d, EGL_EXTENSIONS);

	return !force_readback && wl.udmabuf >= 0 && wl.xbgr && create_image && destroy_image &&
	       gl.EGLImageTargetRenderbufferStorageOES && gl.BlitFramebuffer &&
	       ext && strstr(ext, "EGL_EXT_image_dma_buf_import");
}

static EGLSurface create_window(EGLDisplay d, EGLConfig c, void *native)
{
	struct wl_egl_window *win = native;
	struct shim_surf *s;

	init();
	if (!win || !wl.wl || !wl.shm) {
		shim_error = EGL_BAD_NATIVE_WINDOW;
		return EGL_NO_SURFACE;
	}
	s = calloc(1, sizeof(*s));
	s->dpy = d;
	s->cfg = c;
	s->win = win;
	s->w = win->width > 0 ? win->width : 1;
	s->h = win->height > 0 ? win->height : 1;
	s->interval = 1;
	s->zero_copy = can_zero_copy(d);
	for (int i = 0; i < NBUF; i++)
		s->buf[i].dmafd = -1;
	EGLint pa[] = { EGL_WIDTH, s->w, EGL_HEIGHT, s->h, EGL_NONE };
	s->real = real_eglCreatePbufferSurface(d, c, pa);
	if (s->real == EGL_NO_SURFACE) {
		free(s);
		return EGL_NO_SURFACE;
	}
	s->surf = wl_proxy_create_wrapper(win->surface);
	wl_proxy_set_queue((struct wl_proxy *)s->surf, wl.queue);
	win->driver_private = s;
	win->resize_callback = win_resize;
	win->destroy_window_callback = win_destroy;
	if (stats)
		fprintf(stderr, "egl-wayland-shim: %dx%d window, %s\n", s->w, s->h,
			s->zero_copy ? "dma-buf blit" : "readback");

	pthread_mutex_lock(&lock);
	s->next = surfaces;
	surfaces = s;
	pthread_mutex_unlock(&lock);
	return (EGLSurface)s;
}

EXPORT EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)
{
	return create_window(d, c, (void *)w);
}

EXPORT EGLSurface eglCreatePlatformWindowSurface(EGLDisplay d, EGLConfig c, void *w, const EGLAttrib *a)
{
	return create_window(d, c, w);
}

static EGLSurface create_platform_window_ext(EGLDisplay d, EGLConfig c, void *w, const EGLint *a)
{
	return create_window(d, c, w);
}

EXPORT EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface surf)
{
	struct shim_surf *s = lookup(surf), **pp;

	init();
	if (!s)
		return real_eglDestroySurface(d, surf);
	pthread_mutex_lock(&lock);
	for (pp = &surfaces; *pp; pp = &(*pp)->next)
		if (*pp == s) {
			*pp = s->next;
			break;
		}
	pthread_mutex_unlock(&lock);
	if (s->frame)
		wl_callback_destroy(s->frame);
	free_buffers(s);
	wl_proxy_wrapper_destroy(s->surf);
	if (s->win) {
		s->win->driver_private = NULL;
		s->win->resize_callback = NULL;
		s->win->destroy_window_callback = NULL;
	}
	real_eglDestroySurface(d, s->real);
	free(s);
	return EGL_TRUE;
}

EXPORT EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface w, EGLSurface r, EGLContext c)
{
	init();
	return real_eglMakeCurrent(d, map(w), map(r), c);
}

EXPORT EGLSurface eglGetCurrentSurface(EGLint which)
{
	struct shim_surf *p;
	EGLSurface r;

	init();
	r = real_eglGetCurrentSurface(which);
	pthread_mutex_lock(&lock);
	for (p = surfaces; p && p->real != r; p = p->next)
		;
	pthread_mutex_unlock(&lock);
	return p ? (EGLSurface)p : r;
}

EXPORT EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v)
{
	init();
	return real_eglQuerySurface(d, map(s), a, v);
}

EXPORT EGLBoolean eglSwapInterval(EGLDisplay d, EGLint interval)
{
	struct shim_surf *s;

	init();
	real_eglSwapInterval(d, interval);
	s = lookup(eglGetCurrentSurface(EGL_DRAW));
	if (s)
		s->interval = interval > 0 ? interval : 0;
	return EGL_TRUE;
}

static void frame_done(void *d, struct wl_callback *cb, uint32_t t)
{
	struct shim_surf *s = d;
	wl_callback_destroy(cb);
	s->frame = NULL;
}
static const struct wl_callback_listener frame_listener = { frame_done };

/* Mali hands out ES 3.2 even for ES 2 requests, so ask the context itself */
static int gl_major(void)
{
	const char *v = (const char *)gl.GetString(GL_VERSION);
	return v && !strncmp(v, "OpenGL ES ", 10) ? v[10] - '0' : 2;
}

/* Copy the current default framebuffer into b, leaving the app's GL state as it was */
static void readback(struct shim_surf *s, struct shm_buf *b)
{
	GLint align, fb = 0, pbo = 0, row = 0, skip_r = 0, skip_p = 0, ver = gl_major();

	gl.GetIntegerv(GL_PACK_ALIGNMENT, &align);
	if (ver >= 3) {
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fb);
		gl.GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
		gl.GetIntegerv(GL_PACK_ROW_LENGTH, &row);
		gl.GetIntegerv(GL_PACK_SKIP_ROWS, &skip_r);
		gl.GetIntegerv(GL_PACK_SKIP_PIXELS, &skip_p);
		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		gl.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);
		gl.PixelStorei(GL_PACK_SKIP_ROWS, 0);
		gl.PixelStorei(GL_PACK_SKIP_PIXELS, 0);
	} else {
		gl.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
		gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
	gl.ReadPixels(0, 0, s->w, s->h, wl.xbgr ? GL_RGBA : GL_BGRA_EXT, GL_UNSIGNED_BYTE, b->px);
	gl.PixelStorei(GL_PACK_ALIGNMENT, align);
	if (ver >= 3) {
		gl.PixelStorei(GL_PACK_ROW_LENGTH, row);
		gl.PixelStorei(GL_PACK_SKIP_ROWS, skip_r);
		gl.PixelStorei(GL_PACK_SKIP_PIXELS, skip_p);
		gl.BindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, fb);
	} else {
		gl.BindFramebuffer(GL_FRAMEBUFFER, fb);
	}
}

/* GPU-blit the default framebuffer into b's dma-buf, flipping it upright */
static int blit(struct shim_surf *s, struct shm_buf *b)
{
	EGLContext ctx = real_eglGetCurrentContext();
	GLint draw = 0, read = 0, rb = 0;
	GLboolean scissor, discard;
	struct dma_buf_sync sync;
	GLenum status;

	if (gl_major() < 3) {
		fprintf(stderr, "egl-wayland-shim: blit needs OpenGL ES 3 (%s)\n", gl.GetString(GL_VERSION));
		return -1;
	}
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
	gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	if (b->fb && b->ctx != ctx)
		b->fb = b->rb = 0;	/* objects belong to another context */
	if (!b->fb) {
		gl.GetIntegerv(GL_RENDERBUFFER_BINDING, &rb);
		gl.GenRenderbuffers(1, &b->rb);
		gl.BindRenderbuffer(GL_RENDERBUFFER, b->rb);
		gl.EGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, b->img);
		gl.GenFramebuffers(1, &b->fb);
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, b->fb);
		gl.FramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, b->rb);
		gl.BindRenderbuffer(GL_RENDERBUFFER, rb);
		b->ctx = ctx;
		status = gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			fprintf(stderr, "egl-wayland-shim: dma-buf framebuffer incomplete 0x%x\n", status);
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
			return -1;
		}
	}
	scissor = gl.IsEnabled(GL_SCISSOR_TEST);
	discard = gl.IsEnabled(GL_RASTERIZER_DISCARD);
	if (scissor)
		gl.Disable(GL_SCISSOR_TEST);
	if (discard)
		gl.Disable(GL_RASTERIZER_DISCARD);
	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, b->fb);
	gl.BlitFramebuffer(0, 0, s->w, s->h, 0, s->h, s->w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, read);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
	if (scissor)
		gl.Enable(GL_SCISSOR_TEST);
	if (discard)
		gl.Enable(GL_RASTERIZER_DISCARD);
	gl.Finish();
	/* the compositor reads through the CPU cache */
	sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
	ioctl(b->dmafd, DMA_BUF_IOCTL_SYNC, &sync);
	sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
	ioctl(b->dmafd, DMA_BUF_IOCTL_SYNC, &sync);
	return 0;
}

/* Follow wl_egl_window_resize() on the next frame */
static void resize(struct shim_surf *s)
{
	int nw = s->win->width, nh = s->win->height;

	if (nw <= 0 || nh <= 0 || (nw == s->w && nh == s->h))
		return;
	EGLint pa[] = { EGL_WIDTH, nw, EGL_HEIGHT, nh, EGL_NONE };
	EGLSurface ns = real_eglCreatePbufferSurface(s->dpy, s->cfg, pa);
	if (ns == EGL_NO_SURFACE)
		return;
	EGLSurface draw = real_eglGetCurrentSurface(EGL_DRAW), read = real_eglGetCurrentSurface(EGL_READ);
	if (draw == s->real || read == s->real)
		real_eglMakeCurrent(s->dpy, draw == s->real ? ns : draw, read == s->real ? ns : read,
				    real_eglGetCurrentContext());
	real_eglDestroySurface(s->dpy, s->real);
	s->real = ns;
	s->w = nw;
	s->h = nh;
	free_buffers(s);
}

static EGLBoolean present(struct shim_surf *s)
{
	struct shm_buf *b = NULL;
	double t0;
	int i;

	if (!s->win)
		return EGL_TRUE;
	while (s->frame && s->interval > 0)
		if (wl_display_dispatch_queue(wl.wl, wl.queue) < 0)
			return EGL_FALSE;
	if (!s->buf[0].wl && alloc_buffers(s) < 0) {
		shim_error = EGL_BAD_ALLOC;
		return EGL_FALSE;
	}
	for (;;) {
		for (i = 0; i < NBUF && !b; i++)
			if (!s->buf[i].busy)
				b = &s->buf[i];
		if (b)
			break;
		if (wl_display_dispatch_queue(wl.wl, wl.queue) < 0)
			return EGL_FALSE;
	}

	t0 = now_ms();
	if (s->zero_copy && blit(s, b) < 0) {
		fprintf(stderr, "egl-wayland-shim: dma-buf blit unavailable, reading back\n");
		s->zero_copy = 0;
	}
	if (!s->zero_copy) {
		readback(s, b);
		if (!s->transform_set && wl_proxy_get_version((struct wl_proxy *)s->surf) >= 2) {
			/* GL rows run bottom-up */
			wl_surface_set_buffer_transform(s->surf, WL_OUTPUT_TRANSFORM_FLIPPED_180);
			s->transform_set = 1;
		}
	}
	if (stats) {
		s->cost += now_ms() - t0;
		if (++s->frames == 120) {
			fprintf(stderr, "egl-wayland-shim: %s %.2f ms per frame\n",
				s->zero_copy ? "dma-buf blit" : "readback", s->cost / s->frames);
			s->cost = 0;
			s->frames = 0;
		}
	}
	b->busy = 1;

	if (s->interval > 0) {
		s->frame = wl_surface_frame(s->surf);
		wl_callback_add_listener(s->frame, &frame_listener, s);
	}
	wl_surface_attach(s->surf, b->wl, s->win->dx, s->win->dy);
	s->win->attached_width = s->w;
	s->win->attached_height = s->h;
	s->win->dx = 0;
	s->win->dy = 0;
	if (wl_proxy_get_version((struct wl_proxy *)s->surf) >= 4)
		wl_surface_damage_buffer(s->surf, 0, 0, INT32_MAX, INT32_MAX);
	else
		wl_surface_damage(s->surf, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(s->surf);
	wl_display_flush(wl.wl);
	resize(s);
	return EGL_TRUE;
}

EXPORT EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface surf)
{
	struct shim_surf *s = lookup(surf);

	init();
	return s ? present(s) : real_eglSwapBuffers(d, surf);
}

static EGLBoolean swap_with_damage(EGLDisplay d, EGLSurface surf, const EGLint *rects, EGLint n)
{
	return eglSwapBuffers(d, surf);
}

static EGLBoolean set_damage_region(EGLDisplay d, EGLSurface surf, EGLint *rects, EGLint n)
{
	return EGL_TRUE;
}

EXPORT __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *name)
{
	static const struct {
		const char *name;
		void *fn;
	} own[] = {
		{ "eglChooseConfig", eglChooseConfig },
		{ "eglCreatePlatformWindowSurface", eglCreatePlatformWindowSurface },
		{ "eglCreatePlatformWindowSurfaceEXT", create_platform_window_ext },
		{ "eglCreateWindowSurface", eglCreateWindowSurface },
		{ "eglDestroySurface", eglDestroySurface },
		{ "eglGetConfigAttrib", eglGetConfigAttrib },
		{ "eglGetCurrentSurface", eglGetCurrentSurface },
		{ "eglGetDisplay", eglGetDisplay },
		{ "eglGetError", eglGetError },
		{ "eglGetPlatformDisplay", eglGetPlatformDisplay },
		{ "eglGetPlatformDisplayEXT", get_platform_display_ext },
		{ "eglMakeCurrent", eglMakeCurrent },
		{ "eglQueryString", eglQueryString },
		{ "eglQuerySurface", eglQuerySurface },
		{ "eglSetDamageRegionKHR", set_damage_region },
		{ "eglSwapBuffers", eglSwapBuffers },
		{ "eglSwapBuffersWithDamageEXT", swap_with_damage },
		{ "eglSwapBuffersWithDamageKHR", swap_with_damage },
		{ "eglSwapInterval", eglSwapInterval },
	};
	size_t i;

	init();
	for (i = 0; i < sizeof(own) / sizeof(own[0]); i++)
		if (!strcmp(name, own[i].name))
			return (__eglMustCastToProperFunctionPointerType)own[i].fn;
	return real_eglGetProcAddress ? real_eglGetProcAddress(name) : NULL;
}
