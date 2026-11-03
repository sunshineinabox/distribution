// SPDX-License-Identifier: Apache-2.0
// libEGL.so.1 over libhybris for Wayland EGL apps and wlroots on the Android EGL driver.
// dma-bufs become EGLImages through hybris-gralloc and window surfaces render into linux-dmabuf
// buffers. Without linux-dmabuf, frames go out as wl_shm buffers from pbuffers.
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/dma-buf.h>
#include <linux/udmabuf.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl-backend.h>
#include "hybris-gralloc.h"
#include "hybris-window.h"
#include "linux-dmabuf-v1-client-protocol.h"

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
	X(EGLSurface, eglCreatePbufferFromClientBuffer, (EGLDisplay d, EGLenum t, EGLClientBuffer b, EGLConfig c, const EGLint *a), (d, t, b, c, a)) \
	X(EGLSurface, eglCreatePbufferSurface, (EGLDisplay d, EGLConfig c, const EGLint *a), (d, c, a)) \
	X(EGLSurface, eglCreatePixmapSurface, (EGLDisplay d, EGLConfig c, EGLNativePixmapType p, const EGLint *a), (d, c, p, a)) \
	X(EGLSurface, eglCreatePlatformPixmapSurface, (EGLDisplay d, EGLConfig c, void *p, const EGLAttrib *a), (d, c, p, a)) \
	X(EGLSync, eglCreateSync, (EGLDisplay d, EGLenum t, const EGLAttrib *a), (d, t, a)) \
	X(EGLBoolean, eglDestroyContext, (EGLDisplay d, EGLContext c), (d, c)) \
	X(EGLBoolean, eglDestroySync, (EGLDisplay d, EGLSync s), (d, s)) \
	X(EGLBoolean, eglGetConfigs, (EGLDisplay d, EGLConfig *c, EGLint n, EGLint *r), (d, c, n, r)) \
	X(EGLContext, eglGetCurrentContext, (void), ()) \
	X(EGLDisplay, eglGetCurrentDisplay, (void), ()) \
	X(EGLBoolean, eglGetSyncAttrib, (EGLDisplay d, EGLSync s, EGLint a, EGLAttrib *v), (d, s, a, v)) \
	X(EGLenum, eglQueryAPI, (void), ()) \
	X(EGLBoolean, eglQueryContext, (EGLDisplay d, EGLContext c, EGLint a, EGLint *v), (d, c, a, v)) \
	X(EGLBoolean, eglReleaseTexImage, (EGLDisplay d, EGLSurface s, EGLint b), (d, map(s), b)) \
	X(EGLBoolean, eglReleaseThread, (void), ()) \
	X(EGLBoolean, eglSurfaceAttrib, (EGLDisplay d, EGLSurface s, EGLint a, EGLint v), (d, map(s), a, v)) \
	X(EGLBoolean, eglWaitClient, (void), ()) \
	X(EGLBoolean, eglWaitGL, (void), ()) \
	X(EGLBoolean, eglWaitNative, (EGLint e), (e)) \
	X(EGLBoolean, eglWaitSync, (EGLDisplay d, EGLSync s, EGLint f), (d, s, f))

/* entry points the shim wraps; only the real pointers are generated */
#define WRAPPED(X) \
	X(EGLBoolean, eglChooseConfig, (EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint n, EGLint *r)) \
	X(EGLImage, eglCreateImage, (EGLDisplay d, EGLContext c, EGLenum t, EGLClientBuffer b, const EGLAttrib *a)) \
	X(EGLSurface, eglCreateWindowSurface, (EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)) \
	X(EGLBoolean, eglDestroyImage, (EGLDisplay d, EGLImage i)) \
	X(EGLBoolean, eglDestroySurface, (EGLDisplay d, EGLSurface s)) \
	X(EGLBoolean, eglGetConfigAttrib, (EGLDisplay d, EGLConfig c, EGLint a, EGLint *v)) \
	X(EGLSurface, eglGetCurrentSurface, (EGLint w)) \
	X(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType n)) \
	X(EGLint, eglGetError, (void)) \
	X(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char *n)) \
	X(EGLBoolean, eglInitialize, (EGLDisplay d, EGLint *ma, EGLint *mi)) \
	X(EGLBoolean, eglMakeCurrent, (EGLDisplay d, EGLSurface w, EGLSurface r, EGLContext c)) \
	X(const char *, eglQueryString, (EGLDisplay d, EGLint n)) \
	X(EGLBoolean, eglQuerySurface, (EGLDisplay d, EGLSurface s, EGLint a, EGLint *v)) \
	X(EGLBoolean, eglSwapBuffers, (EGLDisplay d, EGLSurface s)) \
	X(EGLBoolean, eglSwapInterval, (EGLDisplay d, EGLint i)) \
	X(EGLBoolean, eglTerminate, (EGLDisplay d))

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
	struct hyb_window *native;	/* the blob's own window surface over it */
	/* or a pbuffer, presented through wl_shm */
	struct wl_event_queue *queue;	/* the surface's own, like Mesa's */
	struct wl_surface *surf;	/* wrappers on queue */
	struct wl_shm *shm;
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
	struct zwp_linux_dmabuf_v1 *dmabuf;
	struct hyb_wl_format fmt[HYB_NFORMATS];
} wl = { .udmabuf = -1 };

/* EGLImages over hybris-gralloc buffers, which they keep alive */
struct shim_image {
	EGLImage img;
	struct hyb_buffer *buf;
	struct shim_image *next;
};

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct shim_surf *surfaces;
static _Atomic int live_surfaces;
static struct shim_image *images;
static __thread EGLint shim_error;
static int stats, force_readback, gralloc, sync_files, compositor_fences;

/* EGL_ANDROID_blob_cache shader cache: one file per key hash holding the key size, key and
 * value, capped at CACHE_MAX */
#define CACHE_MAX (256LL << 20)
#define CACHE_ENTRY_MAX (4 << 20)

static char cache_dir[PATH_MAX - 64];
static pthread_once_t cache_once = PTHREAD_ONCE_INIT;
static _Atomic long long cache_bytes;

static void cache_init(void)
{
	const char *dir = getenv("HYBRIS_SHIM_SHADER_CACHE"), *xdg = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");

	if (dir && !strcmp(dir, "0"))
		return;
	if (dir)
		snprintf(cache_dir, sizeof(cache_dir), "%s", dir);
	else if (xdg && *xdg)
		snprintf(cache_dir, sizeof(cache_dir), "%s/hybris-mali", xdg);
	else if (home && *home)
		snprintf(cache_dir, sizeof(cache_dir), "%s/.cache/hybris-mali", home);
	else
		return;
	for (char *p = cache_dir + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			mkdir(cache_dir, 0755);
			*p = '/';
		}
	if (mkdir(cache_dir, 0755) && errno != EEXIST)
		cache_dir[0] = 0;
}

static void cache_count(void)
{
	DIR *d = opendir(cache_dir);
	struct dirent *e;
	struct stat st;

	while (d && (e = readdir(d)))
		if (e->d_name[0] != '.' && !fstatat(dirfd(d), e->d_name, &st, 0))
			cache_bytes += st.st_size;
	if (d)
		closedir(d);
}

static void cache_path(char *path, size_t size, const void *key, EGLsizeiANDROID n)
{
	uint64_t h = 0xcbf29ce484222325ULL;	/* FNV-1a */

	for (EGLsizeiANDROID i = 0; i < n; i++)
		h = (h ^ ((const uint8_t *)key)[i]) * 0x100000001b3ULL;
	snprintf(path, size, "%s/%016llx", cache_dir, (unsigned long long)h);
}

static void cache_set(const void *key, EGLsizeiANDROID ksize, const void *value, EGLsizeiANDROID vsize)
{
	char path[PATH_MAX], tmp[PATH_MAX + 32];
	uint32_t n = ksize;
	int fd, ok;

	pthread_once(&cache_once, cache_count);
	if (ksize <= 0 || vsize <= 0 || vsize > CACHE_ENTRY_MAX || cache_bytes + 4 + ksize + vsize > CACHE_MAX)
		return;
	cache_path(path, sizeof(path), key, ksize);
	snprintf(tmp, sizeof(tmp), "%s.%d.%d", path, getpid(), gettid());
	fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (fd < 0)
		return;
	ok = write(fd, &n, 4) == 4 && write(fd, key, ksize) == ksize && write(fd, value, vsize) == vsize;
	close(fd);
	if (ok && !rename(tmp, path))
		cache_bytes += 4 + ksize + vsize;
	else
		unlink(tmp);
}

static EGLsizeiANDROID cache_get(const void *key, EGLsizeiANDROID ksize, void *value, EGLsizeiANDROID vsize)
{
	char path[PATH_MAX];
	EGLsizeiANDROID size = 0;
	struct stat st;
	uint32_t n;
	void *k;
	int fd;

	if (ksize <= 0)
		return 0;
	cache_path(path, sizeof(path), key, ksize);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	k = malloc(ksize);
	if (k && !fstat(fd, &st) && read(fd, &n, 4) == 4 && n == (uint32_t)ksize &&
	    read(fd, k, ksize) == ksize && !memcmp(k, key, ksize)) {
		size = st.st_size - 4 - ksize;
		/* a smaller buffer only asks for the size */
		if (size <= 0 || (size <= vsize && read(fd, value, size) != size))
			size = 0;
	}
	free(k);
	close(fd);
	return size;
}

/* The extension takes the functions once per display */
static void set_blob_cache(EGLDisplay d)
{
	static EGLDisplay done[4];
	PFNEGLSETBLOBCACHEFUNCSANDROIDPROC set = NULL;
	int i;

	if (!cache_dir[0])
		return;
	pthread_mutex_lock(&lock);
	for (i = 0; i < 4 && done[i] && done[i] != d; i++)
		;
	if (i < 4 && !done[i])
		set = (PFNEGLSETBLOBCACHEFUNCSANDROIDPROC)real_eglGetProcAddress("eglSetBlobCacheFuncsANDROID");
	if (set) {
		set(d, cache_set, cache_get);
		done[i] = d;
	}
	pthread_mutex_unlock(&lock);
}

static void load(void)
{
	const char *path = getenv("HYBRIS_SHIM_REAL_EGL");
	const char *copy = getenv("HYBRIS_SHIM_COPY");
	const char *sync = getenv("WLR_GLES2_SYNC_FILES");
	void *h, *g;

	cache_init();
	/* the mapper and thread hooks have to be in place before the blob is loaded */
	gralloc = hyb_gralloc_init() == 0;
	h = dlopen(path ? path : REAL_EGL, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	g = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
	stats = getenv("HYBRIS_SHIM_STATS") != NULL;
	force_readback = copy && !strcmp(copy, "readback");
	/* the compositor waits for and fences dma-bufs through sync files (wlroots 0010) */
	sync_files = sync && !strcmp(sync, "1");
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

/* The blob's own EGL_EXT_image_dma_buf_import does not work, so each dma-buf is wrapped in a
 * hybris-gralloc buffer and imported as a native buffer */

static EGLImage import_dmabuf(EGLDisplay d, const EGLAttrib *a)
{
	EGLAttrib w = 0, h = 0, fourcc = 0, fd = -1, offset = 0, pitch = 0;
	uint32_t lo = 0, hi = 0;
	int have_mod = 0, planes = 1;
	EGLint attr[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
	struct hyb_buffer *buf;
	struct shim_image *si;
	EGLImage img;

	for (; a && a[0] != EGL_NONE; a += 2) {
		switch (a[0]) {
		case EGL_WIDTH: w = a[1]; break;
		case EGL_HEIGHT: h = a[1]; break;
		case EGL_LINUX_DRM_FOURCC_EXT: fourcc = a[1]; break;
		case EGL_DMA_BUF_PLANE0_FD_EXT: fd = a[1]; break;
		case EGL_DMA_BUF_PLANE0_OFFSET_EXT: offset = a[1]; break;
		case EGL_DMA_BUF_PLANE0_PITCH_EXT: pitch = a[1]; break;
		case EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT: lo = a[1]; have_mod = 1; break;
		case EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT: hi = a[1]; have_mod = 1; break;
		case EGL_DMA_BUF_PLANE1_FD_EXT:
		case EGL_DMA_BUF_PLANE2_FD_EXT:
		case EGL_DMA_BUF_PLANE3_FD_EXT:
			planes++;
			break;
		}
	}
	buf = planes == 1 && fd >= 0 ?
		hyb_buffer_wrap(fcntl(fd, F_DUPFD_CLOEXEC, 0), w, h, fourcc,
				have_mod ? (uint64_t)hi << 32 | lo : HYB_MOD_INVALID, offset, pitch) : NULL;
	if (!buf) {
		shim_error = EGL_BAD_MATCH;
		return EGL_NO_IMAGE;
	}
	img = create_image(d, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, hyb_buffer_native(buf), attr);
	si = img != EGL_NO_IMAGE ? malloc(sizeof(*si)) : NULL;
	if (!si) {
		if (img != EGL_NO_IMAGE)
			destroy_image(d, img);
		hyb_buffer_unref(buf);
		return EGL_NO_IMAGE;
	}
	si->img = img;
	si->buf = buf;
	pthread_mutex_lock(&lock);
	si->next = images;
	images = si;
	pthread_mutex_unlock(&lock);
	return img;
}

EXPORT EGLImage eglCreateImage(EGLDisplay d, EGLContext c, EGLenum target, EGLClientBuffer b, const EGLAttrib *a)
{
	EGLint ia[64];
	int i = 0;

	init();
	if (target == EGL_LINUX_DMA_BUF_EXT && gralloc)
		return import_dmabuf(d, a);
	if (real_eglCreateImage)
		return real_eglCreateImage(d, c, target, b, a);
	for (; a && a[0] != EGL_NONE && i < 62; a += 2) {
		ia[i++] = a[0];
		ia[i++] = a[1];
	}
	ia[i] = EGL_NONE;
	return create_image(d, c, target, b, ia);
}

static EGLImageKHR create_image_khr(EGLDisplay d, EGLContext c, EGLenum target, EGLClientBuffer b, const EGLint *ia)
{
	EGLAttrib a[64];
	int i = 0;

	if (target != EGL_LINUX_DMA_BUF_EXT || !gralloc)
		return create_image(d, c, target, b, ia);
	for (; ia && ia[0] != EGL_NONE && i < 62; ia += 2) {
		a[i++] = ia[0];
		a[i++] = ia[1];
	}
	a[i] = EGL_NONE;
	return import_dmabuf(d, a);
}

static EGLBoolean release_image(EGLDisplay d, EGLImage img, EGLBoolean ok)
{
	struct shim_image *si = NULL, **pp;

	pthread_mutex_lock(&lock);
	for (pp = &images; *pp; pp = &(*pp)->next)
		if ((*pp)->img == img) {
			si = *pp;
			*pp = si->next;
			break;
		}
	pthread_mutex_unlock(&lock);
	if (si) {
		hyb_buffer_unref(si->buf);
		free(si);
	}
	return ok;
}

EXPORT EGLBoolean eglDestroyImage(EGLDisplay d, EGLImage img)
{
	init();
	return release_image(d, img, real_eglDestroyImage ? real_eglDestroyImage(d, img) : destroy_image(d, img));
}

static EGLBoolean destroy_image_khr(EGLDisplay d, EGLImageKHR img)
{
	return release_image(d, img, destroy_image(d, img));
}

static EGLBoolean query_dmabuf_formats(EGLDisplay d, EGLint max, EGLint *formats, EGLint *num)
{
	EGLint n = sizeof(hyb_formats) / sizeof(hyb_formats[0]);

	if (max > 0) {
		n = n < max ? n : max;
		for (EGLint i = 0; i < n; i++)
			formats[i] = hyb_formats[i].fourcc;
	}
	*num = n;
	return EGL_TRUE;
}

static EGLBoolean query_dmabuf_modifiers(EGLDisplay d, EGLint format, EGLint max, EGLuint64KHR *mods,
					 EGLBoolean *external_only, EGLint *num)
{
	static const EGLuint64KHR list[] = { HYB_MOD_LINEAR, HYB_MOD_AFBC, HYB_MOD_AFBC_YTR };
	const struct hyb_format *f = hyb_format(format);
	EGLint n;

	if (!f) {
		shim_error = EGL_BAD_PARAMETER;
		return EGL_FALSE;
	}
	n = 1 + f->afbc;
	if (max > 0) {
		n = n < max ? n : max;
		for (EGLint i = 0; i < n; i++) {
			mods[i] = list[i];
			if (external_only)
				external_only[i] = EGL_FALSE;
		}
	}
	*num = n;
	return EGL_TRUE;
}

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
	} else if (!strcmp(iface, zwp_linux_dmabuf_v1_interface.name) && !wl.dmabuf && ver >= 3 && gralloc) {
		/* version 3 lists the modifiers itself */
		wl.dmabuf = wl_registry_bind(r, name, &zwp_linux_dmabuf_v1_interface, 3);
		hyb_window_listen_dmabuf(wl.dmabuf, wl.fmt);
	}
}

/* Window surfaces can be the blob's own over hybris-gralloc buffers */
static int native_windows(void)
{
	return gralloc && wl.dmabuf && wl.fmt[0].supported;
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static int is_wl_display(void *native)
{
	return native && *(const struct wl_interface **)native == &wl_display_interface;
}

/* Drop the objects of a connection that may be gone, without touching them */
static void forget_display(void)
{
	int udmabuf = wl.udmabuf;

	memset(&wl, 0, sizeof(wl));
	wl.udmabuf = udmabuf;
}

static void bind_display(struct wl_display *dpy)
{
	if (wl.wl == dpy)
		return;
	forget_display();
	wl.wl = dpy;
	wl.queue = wl_display_create_queue(dpy);
	wl.wrapper = wl_proxy_create_wrapper(dpy);
	wl_proxy_set_queue((struct wl_proxy *)wl.wrapper, wl.queue);
	wl.registry = wl_display_get_registry(wl.wrapper);
	wl_registry_add_listener(wl.registry, &reg_listener, NULL);
	wl_display_roundtrip_queue(dpy, wl.queue);
	wl_display_roundtrip_queue(dpy, wl.queue);
}

/* At eglTerminate, while the display is still connected as Mesa also needs */
static void unbind_display(void)
{
	if (wl.dmabuf)
		zwp_linux_dmabuf_v1_destroy(wl.dmabuf);
	if (wl.shm)
		wl_shm_destroy(wl.shm);
	wl_registry_destroy(wl.registry);
	wl_proxy_wrapper_destroy(wl.wrapper);
	wl_event_queue_destroy(wl.queue);
	forget_display();
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
	else if (platform != EGL_PLATFORM_WAYLAND_KHR && platform != 0x31DD /* SURFACELESS_MESA */ &&
		 !(platform == EGL_PLATFORM_GBM_KHR && gralloc)) {
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

static int has_ext(const char *list, const char *name, size_t len)
{
	while (list && *list) {
		size_t n = strcspn(list, " ");

		if (n == len && !strncmp(list, name, len))
			return 1;
		list += n + (list[n] == ' ');
	}
	return 0;
}

/* base plus those of the space-separated extensions it lacks */
static void add_exts(char *out, size_t size, const char *base, const char *add)
{
	size_t n = snprintf(out, size, "%s", base ? base : "");

	while (*add && n < size) {
		size_t len = strcspn(add, " ");

		if (!has_ext(base, add, len))
			n += snprintf(out + n, size - n, "%s%.*s", n ? " " : "", (int)len, add);
		add += len + (add[len] == ' ');
	}
}

EXPORT EGLBoolean eglInitialize(EGLDisplay d, EGLint *major, EGLint *minor)
{
	EGLBoolean ok;
	const char *ext;

	init();
	ok = real_eglInitialize(d, major, minor);
	if (!ok)
		return ok;
	/* only a wlroots compositor fences what it renders (wlroots 0010), and with these:
	 * an Xwayland it starts inherits the variable but not the fencing */
	ext = real_eglQueryString(d, EGL_EXTENSIONS);
	compositor_fences = sync_files && dlsym(RTLD_DEFAULT, "wlr_renderer_autocreate") &&
			    has_ext(ext, "EGL_ANDROID_native_fence_sync", 29) && has_ext(ext, "EGL_KHR_wait_sync", 17);
	set_blob_cache(d);
	return ok;
}

EXPORT const char *eglQueryString(EGLDisplay d, EGLint name)
{
	static char client[1024], display[4096];
	const char *s;

	init();
	s = real_eglQueryString(d, name);
	if (name != EGL_EXTENSIONS)
		return s;
	if (d != EGL_NO_DISPLAY) {
		if (!gralloc || !s)
			return s;
		if (!display[0])
			add_exts(display, sizeof(display), s,
				 "EGL_EXT_image_dma_buf_import EGL_EXT_image_dma_buf_import_modifiers");
		return display;
	}
	if (!client[0])
		add_exts(client, sizeof(client), s, gralloc ?
			 "EGL_EXT_platform_base EGL_KHR_platform_wayland EGL_EXT_platform_wayland "
			 "EGL_MESA_platform_surfaceless EGL_KHR_platform_gbm EGL_MESA_platform_gbm" :
			 "EGL_EXT_platform_base EGL_KHR_platform_wayland EGL_EXT_platform_wayland "
			 "EGL_MESA_platform_surfaceless");
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

/* Without native windows, window configs become pbuffer configs */
EXPORT EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *attr, EGLConfig *c, EGLint n, EGLint *r)
{
	EGLint a[128];
	int i = 0, have_type = 0, native;

	init();
	native = native_windows();
	for (; attr && attr[0] != EGL_NONE && i < 124; attr += 2) {
		a[i++] = attr[0];
		a[i] = attr[1];
		if (attr[0] == EGL_SURFACE_TYPE) {
			have_type = 1;
			if (a[i] != EGL_DONT_CARE && (a[i] & EGL_WINDOW_BIT) && !native)
				a[i] = (a[i] & ~EGL_WINDOW_BIT) | EGL_PBUFFER_BIT;
		}
		i++;
	}
	if (!have_type) {
		a[i++] = EGL_SURFACE_TYPE;
		a[i++] = native ? EGL_WINDOW_BIT : EGL_PBUFFER_BIT;
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
			destroy_image_khr(s->dpy, b->img);
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
	b->img = create_image_khr(s->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, a);
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
		if (fd < 0)
			goto fail;
		if (ftruncate(fd, size) < 0) {
			close(fd);
			goto fail;
		}
		/* udmabuf needs the memfd unable to shrink */
		fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
		b->size = size;
		b->px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (b->px == MAP_FAILED) {
			b->px = NULL;
			close(fd);
			goto fail;
		}
		struct wl_shm_pool *pool = wl_shm_create_pool(s->shm, fd, size);
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
	}
	return 0;
fail:
	free_buffers(s);
	return -1;
}

static void win_resize(struct wl_egl_window *w, void *priv) {}

static void win_destroy(void *priv)
{
	struct shim_surf *s = priv;

	if (!s)
		return;
	s->win = NULL;
	if (s->native)
		hyb_window_detach(s->native);
}

static int can_zero_copy(EGLDisplay d)
{
	return !force_readback && gralloc && wl.udmabuf >= 0 && wl.xbgr && create_image && destroy_image &&
	       gl.EGLImageTargetRenderbufferStorageOES && gl.BlitFramebuffer;
}

static const struct wl_callback_listener frame_listener;

/* Attach at the window's pending offset, which wl_surface takes as a request from v5 */
static void attach(struct shim_surf *s, struct wl_buffer *b)
{
	if (wl_proxy_get_version((struct wl_proxy *)s->surf) >= WL_SURFACE_OFFSET_SINCE_VERSION) {
		if (s->win->dx || s->win->dy)
			wl_surface_offset(s->surf, s->win->dx, s->win->dy);
		wl_surface_attach(s->surf, b, 0, 0);
	} else {
		wl_surface_attach(s->surf, b, s->win->dx, s->win->dy);
	}
	s->win->dx = 0;
	s->win->dy = 0;
}

static void free_surface(struct shim_surf *s);

/* The blob's own window surface over a hybris-window; NO_SURFACE to fall back to a pbuffer */
static EGLSurface create_native(struct shim_surf *s, const EGLint *attr)
{
	struct hyb_window_config c = {
		.display = wl.wl, .surface = s->win->surface, .dmabuf = wl.dmabuf, .formats = wl.fmt,
		.egl = s->win, .sync_files = sync_files, .stats = stats,
	};
	EGLSurface real;

	real_eglGetConfigAttrib(s->dpy, s->cfg, EGL_NATIVE_VISUAL_ID, &c.hal_format);
	s->native = hyb_window_create(&c);
	if (!s->native)
		return EGL_NO_SURFACE;
	real = real_eglCreateWindowSurface(s->dpy, s->cfg,
					   (EGLNativeWindowType)(uintptr_t)hyb_window_native(s->native), attr);
	if (real == EGL_NO_SURFACE) {
		hyb_window_detach(s->native);
		hyb_window_unref(s->native);
		s->native = NULL;
	}
	return real;
}

static EGLSurface create_window(EGLDisplay d, EGLConfig c, void *native, const EGLint *attr)
{
	struct wl_egl_window *win = native;
	struct shim_surf *s;

	init();
	if (!win || !wl.wl || !wl.shm) {
		shim_error = EGL_BAD_NATIVE_WINDOW;
		return EGL_NO_SURFACE;
	}
	s = calloc(1, sizeof(*s));
	if (!s) {
		shim_error = EGL_BAD_ALLOC;
		return EGL_NO_SURFACE;
	}
	s->dpy = d;
	s->cfg = c;
	s->win = win;
	s->w = win->width > 0 ? win->width : 1;
	s->h = win->height > 0 ? win->height : 1;
	s->interval = 1;
	for (int i = 0; i < NBUF; i++)
		s->buf[i].dmafd = -1;
	live_surfaces++;
	if (native_windows())
		s->real = create_native(s, attr);
	if (!s->native) {
		EGLint pa[] = { EGL_WIDTH, s->w, EGL_HEIGHT, s->h, EGL_NONE };

		s->queue = wl_display_create_queue(wl.wl);
		s->surf = wl_proxy_create_wrapper(win->surface);
		wl_proxy_set_queue((struct wl_proxy *)s->surf, s->queue);
		s->shm = wl_proxy_create_wrapper(wl.shm);
		wl_proxy_set_queue((struct wl_proxy *)s->shm, s->queue);
		s->zero_copy = can_zero_copy(d);
		s->real = real_eglCreatePbufferSurface(d, c, pa);
	}
	if (s->real == EGL_NO_SURFACE) {
		free_surface(s);
		return EGL_NO_SURFACE;
	}
	win->driver_private = s;
	win->resize_callback = win_resize;
	win->destroy_window_callback = win_destroy;
	if (stats)
		fprintf(stderr, "egl-wayland-shim: %dx%d window, %s\n", s->w, s->h,
			s->native ? "native" : s->zero_copy ? "dma-buf blit" : "readback");

	pthread_mutex_lock(&lock);
	s->next = surfaces;
	surfaces = s;
	pthread_mutex_unlock(&lock);
	return (EGLSurface)s;
}

EXPORT EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)
{
	return create_window(d, c, (void *)w, a);
}

static EGLSurface create_platform_window_ext(EGLDisplay d, EGLConfig c, void *w, const EGLint *a)
{
	return create_window(d, c, w, a);
}

EXPORT EGLSurface eglCreatePlatformWindowSurface(EGLDisplay d, EGLConfig c, void *w, const EGLAttrib *a)
{
	EGLint ia[64];
	int i = 0;

	for (; a && a[0] != EGL_NONE && i < 62; a += 2) {
		ia[i++] = a[0];
		ia[i++] = a[1];
	}
	ia[i] = EGL_NONE;
	return create_window(d, c, w, ia);
}

/* A native window still current anywhere lives on until the blob's last decRef */
static void free_surface(struct shim_surf *s)
{
	if (s->native) {
		hyb_window_detach(s->native);
		hyb_window_unref(s->native);
	}
	if (s->queue) {
		wl_proxy_wrapper_destroy(s->surf);
		wl_proxy_wrapper_destroy(s->shm);
		wl_event_queue_destroy(s->queue);
	}
	free(s);
	live_surfaces--;
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
	s->frame = NULL;
	free_buffers(s);
	if (s->win) {
		s->win->driver_private = NULL;
		s->win->resize_callback = NULL;
		s->win->destroy_window_callback = NULL;
		s->win = NULL;
	}
	if (s->native)
		hyb_window_detach(s->native);
	real_eglDestroySurface(d, s->real);
	free_surface(s);
	return EGL_TRUE;
}

EXPORT EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface w, EGLSurface r, EGLContext c)
{
	EGLContext cur;
	EGLBoolean ok;

	init();
	/* kbase fences no dma-buf, so unless the compositor fences them itself, what a
	 * surfaceless context rendered is finished before anyone else picks it up */
	cur = real_eglGetCurrentContext();
	if (!compositor_fences && cur != EGL_NO_CONTEXT && cur != c &&
	    real_eglGetCurrentSurface(EGL_DRAW) == EGL_NO_SURFACE && gl.Finish)
		gl.Finish();
	ok = real_eglMakeCurrent(d, map(w), map(r), c);
	return ok;
}

/* The blob drops its EGLImages and surfaces here; the Wayland objects go too, so that a
 * client reconnecting gets new ones */
EXPORT EGLBoolean eglTerminate(EGLDisplay d)
{
	struct shim_image *si;
	EGLBoolean ok;

	init();
	ok = real_eglTerminate(d);
	pthread_mutex_lock(&lock);
	while ((si = images)) {
		images = si->next;
		hyb_buffer_unref(si->buf);
		free(si);
	}
	/* surfaces the app left are leaked rather than touched, their display may be gone */
	surfaces = NULL;
	if (wl.wl && !live_surfaces)
		unbind_display();
	else
		forget_display();
	pthread_mutex_unlock(&lock);
	return ok;
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
		if (wl_display_dispatch_queue(wl.wl, s->queue) < 0)
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
		if (wl_display_dispatch_queue(wl.wl, s->queue) < 0)
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
	attach(s, b->wl);
	s->win->attached_width = s->w;
	s->win->attached_height = s->h;
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
	return s && !s->native ? present(s) : real_eglSwapBuffers(d, map(surf));
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
		{ "eglCreateImage", eglCreateImage },
		{ "eglCreateImageKHR", create_image_khr },
		{ "eglCreatePlatformWindowSurface", eglCreatePlatformWindowSurface },
		{ "eglCreatePlatformWindowSurfaceEXT", create_platform_window_ext },
		{ "eglCreateWindowSurface", eglCreateWindowSurface },
		{ "eglDestroyImage", eglDestroyImage },
		{ "eglDestroyImageKHR", destroy_image_khr },
		{ "eglDestroySurface", eglDestroySurface },
		{ "eglGetConfigAttrib", eglGetConfigAttrib },
		{ "eglGetCurrentSurface", eglGetCurrentSurface },
		{ "eglGetDisplay", eglGetDisplay },
		{ "eglGetError", eglGetError },
		{ "eglGetPlatformDisplay", eglGetPlatformDisplay },
		{ "eglGetPlatformDisplayEXT", get_platform_display_ext },
		{ "eglInitialize", eglInitialize },
		{ "eglMakeCurrent", eglMakeCurrent },
		{ "eglQueryDmaBufFormatsEXT", query_dmabuf_formats },
		{ "eglQueryDmaBufModifiersEXT", query_dmabuf_modifiers },
		{ "eglQueryString", eglQueryString },
		{ "eglQuerySurface", eglQuerySurface },
		{ "eglSetDamageRegionKHR", set_damage_region },
		{ "eglSwapBuffers", eglSwapBuffers },
		{ "eglSwapBuffersWithDamageEXT", swap_with_damage },
		{ "eglSwapBuffersWithDamageKHR", swap_with_damage },
		{ "eglSwapInterval", eglSwapInterval },
		{ "eglTerminate", eglTerminate },
	};
	size_t i;

	init();
	for (i = 0; i < sizeof(own) / sizeof(own[0]); i++)
		if (!strcmp(name, own[i].name))
			return (__eglMustCastToProperFunctionPointerType)own[i].fn;
	return real_eglGetProcAddress ? real_eglGetProcAddress(name) : NULL;
}
