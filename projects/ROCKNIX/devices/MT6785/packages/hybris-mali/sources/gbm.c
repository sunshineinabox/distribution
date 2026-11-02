// SPDX-License-Identifier: Apache-2.0
// libgbm.so.1 for the stock Mali blob: buffers are udmabufs laid out so that both the
// blob and the MediaTek display read them, linear or AFBC, and compositors render into
// them through the EGL shim. GBM window surfaces are not offered.
#define _GNU_SOURCE
#include <errno.h>
#include <gbm.h>
#include <linux/dma-buf.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#define HYBRIS_GRALLOC_LAYOUT_ONLY
#include "hybris-gralloc.h"

#define EXPORT __attribute__((visibility("default")))

struct gbm_device {
	int fd;
};

struct gbm_bo {
	struct gbm_device *gbm;
	uint32_t width, height, format, pitch, offset, handle;
	uint64_t modifier;
	int fd;
	size_t size;
	void *user_data;
	void (*destroy_user_data)(struct gbm_bo *, void *);
};

struct mapping {
	void *ptr;
	size_t size;
	uint64_t sync;
};

static uint32_t canonical(uint32_t format)
{
	if (format == GBM_BO_FORMAT_XRGB8888)
		return GBM_FORMAT_XRGB8888;
	if (format == GBM_BO_FORMAT_ARGB8888)
		return GBM_FORMAT_ARGB8888;
	return format;
}

EXPORT struct gbm_device *gbm_create_device(int fd)
{
	struct gbm_device *gbm = calloc(1, sizeof(*gbm));

	if (gbm)
		gbm->fd = fd;
	return gbm;
}

EXPORT void gbm_device_destroy(struct gbm_device *gbm)
{
	free(gbm);
}

EXPORT int gbm_device_get_fd(struct gbm_device *gbm)
{
	return gbm->fd;
}

EXPORT const char *gbm_device_get_backend_name(struct gbm_device *gbm)
{
	return "hybris";
}

EXPORT int gbm_device_is_format_supported(struct gbm_device *gbm, uint32_t format, uint32_t flags)
{
	return hyb_format(canonical(format)) != NULL;
}

EXPORT int gbm_device_get_format_modifier_plane_count(struct gbm_device *gbm, uint32_t format,
							uint64_t modifier)
{
	return hyb_supported(hyb_format(canonical(format)), modifier) ? 1 : -1;
}

static struct gbm_bo *new_bo(struct gbm_device *gbm, int fd, uint32_t width, uint32_t height,
			     uint32_t format, uint64_t modifier, uint32_t pitch, uint32_t offset)
{
	struct gbm_bo *bo = calloc(1, sizeof(*bo));

	if (!bo) {
		close(fd);
		errno = ENOMEM;
		return NULL;
	}
	bo->gbm = gbm;
	bo->fd = fd;
	bo->width = width;
	bo->height = height;
	bo->format = format;
	bo->modifier = modifier;
	bo->pitch = pitch;
	bo->offset = offset;
	bo->size = lseek(fd, 0, SEEK_END);
	return bo;
}

static struct gbm_bo *create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format,
			     const uint64_t *modifiers, unsigned int count, uint32_t flags)
{
	const struct hyb_format *f = hyb_format(canonical(format));
	uint64_t modifier;
	uint32_t pitch;
	size_t size;
	int fd;

	if (!f || !width || !height) {
		errno = EINVAL;
		return NULL;
	}
	modifier = hyb_pick_modifier(f, width, height, modifiers, count);
	/* CPU-written buffers such as cursors stay linear */
	if (modifier != HYB_MOD_INVALID && (flags & (GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE)))
		modifier = hyb_pick_modifier(f, width, height, NULL, 0);
	if (modifier == HYB_MOD_INVALID) {
		errno = EINVAL;
		return NULL;
	}
	size = hyb_layout(width, height, f, modifier, &pitch);
	fd = hyb_dmabuf_alloc(size);
	if (fd < 0) {
		errno = ENOMEM;
		return NULL;
	}
	return new_bo(gbm, fd, width, height, f->fourcc, modifier, pitch, 0);
}

EXPORT struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width, uint32_t height,
				    uint32_t format, uint32_t flags)
{
	return create(gbm, width, height, format, NULL, 0, flags);
}

EXPORT struct gbm_bo *gbm_bo_create_with_modifiers(struct gbm_device *gbm, uint32_t width, uint32_t height,
						   uint32_t format, const uint64_t *modifiers,
						   const unsigned int count)
{
	return create(gbm, width, height, format, modifiers, count, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
}

EXPORT struct gbm_bo *gbm_bo_create_with_modifiers2(struct gbm_device *gbm, uint32_t width, uint32_t height,
						    uint32_t format, const uint64_t *modifiers,
						    const unsigned int count, uint32_t flags)
{
	return create(gbm, width, height, format, modifiers, count, flags);
}

EXPORT struct gbm_bo *gbm_bo_import(struct gbm_device *gbm, uint32_t type, void *buffer, uint32_t flags)
{
	if (type == GBM_BO_IMPORT_FD) {
		struct gbm_import_fd_data *d = buffer;

		return new_bo(gbm, fcntl(d->fd, F_DUPFD_CLOEXEC, 0), d->width, d->height, canonical(d->format),
			      HYB_MOD_INVALID, d->stride, 0);
	}
	if (type == GBM_BO_IMPORT_FD_MODIFIER) {
		struct gbm_import_fd_modifier_data *d = buffer;

		if (d->num_fds == 1)
			return new_bo(gbm, fcntl(d->fds[0], F_DUPFD_CLOEXEC, 0), d->width, d->height,
				      canonical(d->format), d->modifier, d->strides[0], d->offsets[0]);
	}
	errno = EINVAL;
	return NULL;
}

EXPORT void *gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t flags, uint32_t *stride, void **map_data)
{
	const struct hyb_format *f = hyb_format(bo->format);
	struct mapping *m;
	struct dma_buf_sync sync;

	if (!f || hyb_is_afbc(bo->modifier) || !(m = calloc(1, sizeof(*m)))) {
		errno = EINVAL;
		return NULL;
	}
	m->size = bo->size;
	m->sync = (flags & GBM_BO_TRANSFER_READ ? DMA_BUF_SYNC_READ : 0) |
		  (flags & GBM_BO_TRANSFER_WRITE ? DMA_BUF_SYNC_WRITE : 0);
	m->ptr = mmap(NULL, m->size, PROT_READ | (flags & GBM_BO_TRANSFER_WRITE ? PROT_WRITE : 0),
		      MAP_SHARED, bo->fd, 0);
	if (m->ptr == MAP_FAILED) {
		free(m);
		return NULL;
	}
	sync.flags = DMA_BUF_SYNC_START | m->sync;
	ioctl(bo->fd, DMA_BUF_IOCTL_SYNC, &sync);
	*stride = bo->pitch;
	*map_data = m;
	return (char *)m->ptr + bo->offset + (size_t)y * bo->pitch + (size_t)x * f->cpp;
}

EXPORT void gbm_bo_unmap(struct gbm_bo *bo, void *map_data)
{
	struct mapping *m = map_data;
	struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_END | m->sync };

	ioctl(bo->fd, DMA_BUF_IOCTL_SYNC, &sync);
	munmap(m->ptr, m->size);
	free(m);
}

EXPORT int gbm_bo_write(struct gbm_bo *bo, const void *buf, size_t count)
{
	uint32_t stride;
	void *data, *p = gbm_bo_map(bo, 0, 0, bo->width, bo->height, GBM_BO_TRANSFER_WRITE, &stride, &data);

	if (!p)
		return -1;
	memcpy(p, buf, count < bo->size - bo->offset ? count : bo->size - bo->offset);
	gbm_bo_unmap(bo, data);
	return 0;
}

EXPORT uint32_t gbm_bo_get_width(struct gbm_bo *bo)
{
	return bo->width;
}

EXPORT uint32_t gbm_bo_get_height(struct gbm_bo *bo)
{
	return bo->height;
}

EXPORT uint32_t gbm_bo_get_stride(struct gbm_bo *bo)
{
	return bo->pitch;
}

EXPORT uint32_t gbm_bo_get_stride_for_plane(struct gbm_bo *bo, int plane)
{
	return plane ? 0 : bo->pitch;
}

EXPORT uint32_t gbm_bo_get_format(struct gbm_bo *bo)
{
	return bo->format;
}

EXPORT uint32_t gbm_bo_get_bpp(struct gbm_bo *bo)
{
	const struct hyb_format *f = hyb_format(bo->format);

	return f ? f->cpp * 8 : 0;
}

EXPORT uint32_t gbm_bo_get_offset(struct gbm_bo *bo, int plane)
{
	return plane ? 0 : bo->offset;
}

EXPORT struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo)
{
	return bo->gbm;
}

EXPORT uint64_t gbm_bo_get_modifier(struct gbm_bo *bo)
{
	return bo->modifier;
}

EXPORT int gbm_bo_get_plane_count(struct gbm_bo *bo)
{
	return 1;
}

/* A GEM handle on the device's DRM fd, imported on first use */
EXPORT union gbm_bo_handle gbm_bo_get_handle_for_plane(struct gbm_bo *bo, int plane)
{
	union gbm_bo_handle h = { .s32 = -1 };

	if (plane || (!bo->handle && drmPrimeFDToHandle(bo->gbm->fd, bo->fd, &bo->handle)))
		return h;
	h.u32 = bo->handle;
	return h;
}

EXPORT union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo)
{
	return gbm_bo_get_handle_for_plane(bo, 0);
}

EXPORT int gbm_bo_get_fd_for_plane(struct gbm_bo *bo, int plane)
{
	return plane ? -1 : fcntl(bo->fd, F_DUPFD_CLOEXEC, 0);
}

EXPORT int gbm_bo_get_fd(struct gbm_bo *bo)
{
	return gbm_bo_get_fd_for_plane(bo, 0);
}

EXPORT void gbm_bo_set_user_data(struct gbm_bo *bo, void *data, void (*destroy_user_data)(struct gbm_bo *, void *))
{
	bo->user_data = data;
	bo->destroy_user_data = destroy_user_data;
}

EXPORT void *gbm_bo_get_user_data(struct gbm_bo *bo)
{
	return bo->user_data;
}

EXPORT void gbm_bo_destroy(struct gbm_bo *bo)
{
	if (bo->destroy_user_data)
		bo->destroy_user_data(bo, bo->user_data);
	if (bo->handle)
		drmCloseBufferHandle(bo->gbm->fd, bo->handle);
	close(bo->fd);
	free(bo);
}

EXPORT struct gbm_surface *gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height,
					      uint32_t format, uint32_t flags)
{
	errno = ENOSYS;
	return NULL;
}

EXPORT struct gbm_surface *gbm_surface_create_with_modifiers(struct gbm_device *gbm, uint32_t width,
							     uint32_t height, uint32_t format,
							     const uint64_t *modifiers, const unsigned int count)
{
	errno = ENOSYS;
	return NULL;
}

EXPORT struct gbm_surface *gbm_surface_create_with_modifiers2(struct gbm_device *gbm, uint32_t width,
							      uint32_t height, uint32_t format,
							      const uint64_t *modifiers, const unsigned int count,
							      uint32_t flags)
{
	errno = ENOSYS;
	return NULL;
}

EXPORT struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface)
{
	return NULL;
}

EXPORT void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo)
{
}

EXPORT int gbm_surface_has_free_buffers(struct gbm_surface *surface)
{
	return 0;
}

EXPORT void gbm_surface_destroy(struct gbm_surface *surface)
{
}

EXPORT char *gbm_format_get_name(uint32_t gbm_format, struct gbm_format_name_desc *desc)
{
	uint32_t f = canonical(gbm_format);

	desc->name[0] = f;
	desc->name[1] = f >> 8;
	desc->name[2] = f >> 16;
	desc->name[3] = f >> 24;
	desc->name[4] = 0;
	return desc->name;
}
