// SPDX-License-Identifier: Apache-2.0
// Gralloc buffers for the stock Mali blob: dma-bufs described to it by hybris-gralloc's IMapper@4.0.
// The layout rules are shared with libgbm, which allocates what the mapper later describes.
#ifndef HYBRIS_GRALLOC_H
#define HYBRIS_GRALLOC_H

#include <fcntl.h>
#include <linux/udmabuf.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define HYB_FOURCC(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define HYB_XBGR8888 HYB_FOURCC('X', 'B', '2', '4')
#define HYB_ABGR8888 HYB_FOURCC('A', 'B', '2', '4')
#define HYB_XRGB8888 HYB_FOURCC('X', 'R', '2', '4')
#define HYB_ARGB8888 HYB_FOURCC('A', 'R', '2', '4')
#define HYB_RGB565 HYB_FOURCC('R', 'G', '1', '6')

#define HYB_MOD_LINEAR 0ULL
#define HYB_MOD_INVALID 0x00ffffffffffffffULL
/* DRM_FORMAT_MOD_ARM_AFBC(32x8 | SPLIT | SPARSE), and with YTR */
#define HYB_MOD_AFBC 0x0800000000000062ULL
#define HYB_MOD_AFBC_YTR 0x0800000000000072ULL

/* AFBC superblocks and the Arm header-to-body alignment the blob writes */
#define HYB_AFBC_W 32
#define HYB_AFBC_H 8
#define HYB_AFBC_BODY_ALIGN 1024

struct hyb_format {
	uint32_t fourcc;
	uint32_t blob_fourcc;	/* what Arm gralloc calls the HAL format */
	int hal;		/* HAL_PIXEL_FORMAT_* the blob renders */
	int cpp;
	int afbc;		/* 1: AFBC, 2: AFBC with YTR too */
};

/* Bifrost writes no BGR-ordered AFBC, so BGRA buffers stay linear */
static const struct hyb_format hyb_formats[] = {
	{ HYB_XBGR8888, HYB_XBGR8888, 2, 4, 2 },	/* RGBX_8888 */
	{ HYB_ABGR8888, HYB_ABGR8888, 1, 4, 2 },	/* RGBA_8888 */
	{ HYB_XRGB8888, HYB_ARGB8888, 5, 4, 0 },	/* BGRA_8888 */
	{ HYB_ARGB8888, HYB_ARGB8888, 5, 4, 0 },
	{ HYB_RGB565, HYB_RGB565, 4, 2, 0 },		/* RGB_565 */
};

static inline const struct hyb_format *hyb_format(uint32_t fourcc)
{
	for (size_t i = 0; i < sizeof(hyb_formats) / sizeof(hyb_formats[0]); i++)
		if (hyb_formats[i].fourcc == fourcc)
			return &hyb_formats[i];
	return NULL;
}

static inline int hyb_is_afbc(uint64_t modifier)
{
	return modifier == HYB_MOD_AFBC || modifier == HYB_MOD_AFBC_YTR;
}

static inline int hyb_supported(const struct hyb_format *f, uint64_t modifier)
{
	return f && (modifier == HYB_MOD_LINEAR || modifier == HYB_MOD_INVALID ||
		     (f->afbc && modifier == HYB_MOD_AFBC) || (f->afbc == 2 && modifier == HYB_MOD_AFBC_YTR));
}

static inline size_t hyb_afbc_blocks(int w, int h)
{
	return (size_t)((w + HYB_AFBC_W - 1) / HYB_AFBC_W) * ((h + HYB_AFBC_H - 1) / HYB_AFBC_H);
}

/* The MediaTek display finds the body 128 bytes past the headers, so the blob's AFBC
 * can only be scanned out at sizes where both alignments land on the same offset */
static inline int hyb_afbc_scanout_ok(int w, int h)
{
	size_t hdr = hyb_afbc_blocks(w, h) * 16;

	return (hdr + 127) / 128 * 128 == (hdr + HYB_AFBC_BODY_ALIGN - 1) / HYB_AFBC_BODY_ALIGN * HYB_AFBC_BODY_ALIGN;
}

/* The modifier to allocate from a list, LINEAR for an implicit one; INVALID if none fits */
static inline uint64_t hyb_pick_modifier(const struct hyb_format *f, int w, int h,
					 const uint64_t *mods, unsigned int n)
{
	int linear = !n, afbc = 0, ytr = 0;

	for (unsigned int i = 0; i < n; i++) {
		linear |= mods[i] == HYB_MOD_LINEAR || mods[i] == HYB_MOD_INVALID;
		afbc |= mods[i] == HYB_MOD_AFBC;
		ytr |= mods[i] == HYB_MOD_AFBC_YTR;
	}
	if (f->afbc && hyb_afbc_scanout_ok(w, h)) {
		if (ytr && f->afbc == 2)
			return HYB_MOD_AFBC_YTR;
		if (afbc)
			return HYB_MOD_AFBC;
	}
	return linear ? HYB_MOD_LINEAR : HYB_MOD_INVALID;
}

/* Bytes to allocate and the pitch reported for one plane */
static inline size_t hyb_layout(int w, int h, const struct hyb_format *f, uint64_t modifier,
				uint32_t *pitch)
{
	size_t size;

	if (hyb_is_afbc(modifier)) {
		size_t blocks = hyb_afbc_blocks(w, h);

		*pitch = (w + HYB_AFBC_W - 1) / HYB_AFBC_W * HYB_AFBC_W * f->cpp;
		size = (blocks * 16 + HYB_AFBC_BODY_ALIGN - 1) / HYB_AFBC_BODY_ALIGN * HYB_AFBC_BODY_ALIGN +
		       blocks * HYB_AFBC_W * HYB_AFBC_H * f->cpp;
	} else {
		*pitch = (w * f->cpp + 63) & ~63;
		size = (size_t)*pitch * h;
	}
	return (size + 4095) & ~(size_t)4095;
}

/* A udmabuf over a sealed memfd: the GPU, the display and the CPU can all map it */
static inline int hyb_dmabuf_alloc(size_t size)
{
	int memfd = memfd_create("hybris-gralloc", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	struct udmabuf_create c = { .memfd = memfd, .flags = UDMABUF_FLAGS_CLOEXEC, .size = size };
	int dev, fd;

	if (memfd < 0)
		return -1;
	if (ftruncate(memfd, size) < 0 || fcntl(memfd, F_ADD_SEALS, F_SEAL_SHRINK) < 0) {
		close(memfd);
		return -1;
	}
	dev = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
	fd = dev < 0 ? -1 : ioctl(dev, UDMABUF_CREATE, &c);
	if (dev >= 0)
		close(dev);
	close(memfd);
	return fd;
}

#ifndef HYBRIS_GRALLOC_LAYOUT_ONLY
struct hyb_buffer;

/* Install the IMapper hook; must run before the blob is loaded */
int hyb_gralloc_init(void);

/* Wrap one dma-buf plane, taking ownership of fd; NULL if the blob cannot use it */
struct hyb_buffer *hyb_buffer_wrap(int fd, int width, int height, uint32_t fourcc,
				   uint64_t modifier, uint32_t offset, uint32_t pitch);
struct hyb_buffer *hyb_buffer_alloc(int width, int height, uint32_t fourcc, uint64_t modifier);
void hyb_buffer_unref(struct hyb_buffer *b);

/* The ANativeWindowBuffer to hand the blob as EGL_NATIVE_BUFFER_ANDROID */
void *hyb_buffer_native(struct hyb_buffer *b);
int hyb_buffer_fd(const struct hyb_buffer *b);
uint32_t hyb_buffer_format(const struct hyb_buffer *b);
uint32_t hyb_buffer_pitch(const struct hyb_buffer *b);
uint64_t hyb_buffer_modifier(const struct hyb_buffer *b);
#endif

#endif
