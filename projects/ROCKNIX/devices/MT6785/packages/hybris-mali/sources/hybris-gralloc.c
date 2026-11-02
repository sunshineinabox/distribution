// SPDX-License-Identifier: Apache-2.0
// IMapper@4.0 for the stock Mali blob, handed to it in place of the HIDL service through a
// libhybris symbol hook, so it imports plain dma-bufs as EGL_NATIVE_BUFFER_ANDROID buffers
// without an Android allocator or servicemanager.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hybris-gralloc.h"

#define HIDDEN __attribute__((visibility("hidden")))

static void *(*android_dlopen)(const char *name, int flags);
static void *(*android_dlsym)(void *handle, const char *sym);
static void (*set_hook)(void *(*cb)(const char *sym, const char *requester));
static int debug;

/* HIDL / libutils object layouts on Android 11 arm64 */
struct hidl_string { const char *buf; uint32_t size; uint8_t owns; };
struct metadata_type { struct hidl_string name; int64_t value; };
struct hidl_vec { const void *buf; uint32_t size; uint8_t owns; };
struct hidl_handle { const void *handle; uint8_t owns; };
/* Return<void> / Return<Error>: Status {exception, error, String8} + checked flag, value in the tail */
struct ret { int32_t exception, error_code; void *message; uint8_t checked; uint8_t pad[3]; int32_t val; };

enum { ERR_NONE = 0, ERR_BAD_BUFFER = 2, ERR_UNSUPPORTED = 7 };

static void (*string8_ctor)(void *s8);
static void (*refbase_ctor)(void *rb);
static void (*refbase_incstrong)(const void *rb, const void *id);

/* IMapper object: the slot vtable, then a real RefBase as the virtual base at +16 */
static void *vtbl[3 + 32];
static struct { void **vptr; uint64_t pad; void *rb_vptr; void *rb_refs; } mapper;

/* native_handle_t: header, the dma-buf fd, then HYB_NUM_INTS ints */
struct hyb_handle {
	int version, num_fds, num_ints;
	int fd;
	int magic, width, height, stride, fourcc, format;
	uint32_t mod_lo, mod_hi, usage_lo, usage_hi, size, id;
};
#define HYB_MAGIC 0x4859424d
#define HYB_NUM_INTS 12

/* ANativeWindowBuffer on arm64 */
struct anw_buffer {
	int magic, version;
	void *reserved[4];
	void (*inc_ref)(void *);
	void (*dec_ref)(void *);
	int width, height, stride, format, usage_deprecated;
	uintptr_t layer_count;
	void *reserved2;
	const void *handle;
	uint64_t usage;
	void *reserved_proc[7];
};

struct hyb_buffer {
	struct anw_buffer anw;	/* first: the blob's incRef/decRef get its address */
	atomic_int refs;
	uint32_t fourcc, pitch;
	uint64_t modifier;
	struct hyb_handle handle;
};

static void ret_ok(struct ret *r, int32_t val)
{
	memset(r, 0, sizeof(*r));
	string8_ctor(&r->message);
	r->val = val;
}

/* libc++ std::function: __f_ at +32, operator() is vtable slot 6, arguments by reference */
static void call_cb(const void *fn, void *a, void *b, void *c)
{
	void *f = *(void **)((const char *)fn + 32);
	void (*op)(void *, void *, void *, void *) = (void (*)(void *, void *, void *, void *))(*(void ***)f)[6];

	op(f, a, b, c);
}

struct enc { uint8_t buf[1024]; uint32_t n; };

static void put(struct enc *e, const void *p, size_t n) { memcpy(e->buf + e->n, p, n); e->n += n; }
static void put64(struct enc *e, int64_t v) { put(e, &v, 8); }
static void put32(struct enc *e, int32_t v) { put(e, &v, 4); }
static void putstr(struct enc *e, const char *str) { put64(e, strlen(str)); put(e, str, strlen(str)); }

static const char std_type[] = "android.hardware.graphics.common.StandardMetadataType";
static const char arm_type[] = "arm.graphics.ArmMetadataType";
static const char comp_type[] = "android.hardware.graphics.common.PlaneLayoutComponentType";

static int is_type(const struct metadata_type *t, const char *name)
{
	return t->name.size == strlen(name) && !memcmp(t->name.buf, name, t->name.size);
}

/* PlaneLayoutComponent: R 1<<10, G 1<<11, B 1<<12, A 1<<30 with bit offset and size */
static void put_components(struct enc *e, uint32_t fourcc)
{
	static const struct { uint32_t fourcc; int n; int64_t c[4][3]; } layouts[] = {
		{ HYB_XBGR8888, 3, { { 1 << 10, 0, 8 }, { 1 << 11, 8, 8 }, { 1 << 12, 16, 8 } } },
		{ HYB_ABGR8888, 4, { { 1 << 10, 0, 8 }, { 1 << 11, 8, 8 }, { 1 << 12, 16, 8 }, { 1 << 30, 24, 8 } } },
		{ HYB_ARGB8888, 4, { { 1 << 12, 0, 8 }, { 1 << 11, 8, 8 }, { 1 << 10, 16, 8 }, { 1 << 30, 24, 8 } } },
		{ HYB_RGB565, 3, { { 1 << 12, 0, 5 }, { 1 << 11, 5, 6 }, { 1 << 10, 11, 5 } } },
	};

	for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++) {
		if (layouts[i].fourcc != fourcc)
			continue;
		put64(e, layouts[i].n);
		for (int j = 0; j < layouts[i].n; j++) {
			putstr(e, comp_type);
			put64(e, layouts[i].c[j][0]);
			put64(e, layouts[i].c[j][1]);
			put64(e, layouts[i].c[j][2]);
		}
		return;
	}
	put64(e, 0);
}

/* gralloc4 encoding of one standard metadata value, UNSUPPORTED for the rest */
static int encode_std(const struct hyb_handle *h, const struct metadata_type *t, struct enc *e)
{
	uint64_t mod = (uint64_t)h->mod_hi << 32 | h->mod_lo;
	int afbc = hyb_is_afbc(mod);
	int bw = afbc ? HYB_AFBC_W : 1, bh = afbc ? HYB_AFBC_H : 1;

	put64(e, t->name.size);
	put(e, t->name.buf, t->name.size);
	put64(e, t->value);
	switch (t->value) {
	case 1: put64(e, h->id); break;					/* BUFFER_ID */
	case 2: putstr(e, "hybris"); break;				/* NAME */
	case 3: put64(e, h->width); break;				/* WIDTH */
	case 4: put64(e, h->height); break;				/* HEIGHT */
	case 5: put64(e, 1); break;					/* LAYER_COUNT */
	case 6: put32(e, h->format); break;				/* PIXEL_FORMAT_REQUESTED */
	case 7: put32(e, h->fourcc); break;				/* PIXEL_FORMAT_FOURCC */
	case 8: put64(e, mod); break;					/* PIXEL_FORMAT_MODIFIER */
	case 9: put64(e, (uint64_t)h->usage_hi << 32 | h->usage_lo); break;	/* USAGE */
	case 10: put64(e, h->size); break;				/* ALLOCATION_SIZE */
	case 11: put64(e, 0); break;					/* PROTECTED_CONTENT */
	case 12:							/* COMPRESSION */
		putstr(e, afbc ? "arm.graphics.Compression" : "android.hardware.graphics.common.Compression");
		put64(e, 0);
		break;
	case 13: putstr(e, "android.hardware.graphics.common.Interlaced"); put64(e, 0); break;
	case 14: putstr(e, "android.hardware.graphics.common.ChromaSiting"); put64(e, 0); break;
	case 15:							/* PLANE_LAYOUTS */
		put64(e, 1);
		put_components(e, h->fourcc);
		put64(e, 0);						/* offsetInBytes */
		put64(e, hyb_format(h->fourcc)->cpp * 8);		/* sampleIncrementInBits */
		put64(e, h->stride);					/* strideInBytes */
		put64(e, (h->width + bw - 1) / bw * bw);		/* widthInSamples */
		put64(e, (h->height + bh - 1) / bh * bh);		/* heightInSamples */
		put64(e, h->size);					/* totalSizeInBytes */
		put64(e, 1);						/* subsampling */
		put64(e, 1);
		break;
	case 16:							/* CROP */
		put64(e, 1);
		put32(e, 0);
		put32(e, 0);
		put32(e, h->width);
		put32(e, h->height);
		break;
	case 17: put32(e, 0); break;					/* DATASPACE */
	case 18: put32(e, 0); break;					/* BLEND_MODE */
	default:
		return ERR_UNSUPPORTED;
	}
	return ERR_NONE;
}

static const struct hyb_handle *handle_of(uint64_t p)
{
	const struct hyb_handle *h = (const void *)p;

	return h && h->magic == HYB_MAGIC && h->num_fds == 1 && h->num_ints == HYB_NUM_INTS ? h : NULL;
}

static void mapper_get(const struct hyb_handle *h, const struct metadata_type *t, const void *cb)
{
	struct enc e = { .n = 0 };
	int err = ERR_UNSUPPORTED;
	struct hidl_vec v;

	if (!h)
		err = ERR_BAD_BUFFER;
	else if (is_type(t, std_type))
		err = encode_std(h, t, &e);
	else if (is_type(t, arm_type) && t->value == 1) {	/* PLANE_FDS, no header */
		put64(&e, 1);
		put64(&e, h->fd);
		err = ERR_NONE;
	}
	if (err)
		e.n = 0;
	if (debug)
		fprintf(stderr, "hybris-gralloc: get(%.*s %lld) = %d\n", (int)t->name.size, t->name.buf,
			(long long)t->value, err);
	v.buf = e.buf;
	v.size = e.n;
	v.owns = 0;
	call_cb(cb, &err, &v, NULL);
}

/* importBuffer clones the raw handle like a real mapper, so the blob's copy can outlive the buffer */
static void mapper_import(const struct hidl_handle *raw, const void *cb)
{
	const struct hyb_handle *h = handle_of((uint64_t)raw->handle);
	struct hyb_handle *c = NULL;
	int err = ERR_BAD_BUFFER;

	if (h && (c = malloc(sizeof(*c)))) {
		*c = *h;
		c->fd = fcntl(h->fd, F_DUPFD_CLOEXEC, 0);
		if (c->fd >= 0)
			err = ERR_NONE;
		else {
			free(c);
			c = NULL;
		}
	}
	call_cb(cb, &err, &c, NULL);
}

static int mapper_free(uint64_t p)
{
	struct hyb_handle *h = (struct hyb_handle *)handle_of(p);

	if (!h)
		return ERR_BAD_BUFFER;
	close(h->fd);
	h->magic = 0;
	free(h);
	return ERR_NONE;
}

static const char *slot_name[] = {
	"~IBase", "~IBase(del)", "isRemote", "interfaceChain", "debug", "interfaceDescriptor",
	"getHashChain", "setHALInstrumentation", "linkToDeath", "ping", "getDebugInfo",
	"notifySyspropsChanged", "unlinkToDeath", "createDescriptor", "importBuffer", "freeBuffer",
	"validateBufferSize", "getTransportSize", "lock", "unlock", "flushLockedBuffer",
	"rereadLockedBuffer", "isSupported", "get", "set", "getFromBufferDescriptorInfo",
	"listSupportedMetadataTypes", "dumpBuffer", "dumpBuffers", "getReservedRegion", "?", "?",
};

/* x0 = this, x1..x5 = arguments, x6 = slot, x7 = the caller's x8 result pointer */
HIDDEN uint64_t slot_dispatch(void *self, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
			      uint64_t a5, uint64_t slot, struct ret *sret)
{
	int err = ERR_NONE;
	uint32_t nfds = 1, nints = HYB_NUM_INTS;
	uint8_t yes = 1;
	void *data = NULL;
	struct hidl_handle no_fence = { 0 };

	if (slot <= 2)		/* destructors and isRemote() return plainly */
		return 0;
	switch (slot) {
	case 14:
		mapper_import((const void *)a1, (const void *)a2);
		break;
	case 15:
		err = mapper_free(a1);
		break;
	case 16:		/* validateBufferSize */
		err = handle_of(a1) ? ERR_NONE : ERR_BAD_BUFFER;
		break;
	case 17:		/* getTransportSize */
		err = handle_of(a1) ? ERR_NONE : ERR_BAD_BUFFER;
		call_cb((const void *)a2, &err, &nfds, &nints);
		break;
	case 22:		/* isSupported */
		call_cb((const void *)a2, &err, &yes, NULL);
		break;
	case 23:
		mapper_get(handle_of(a1), (const void *)a2, (const void *)a3);
		break;
	case 24:		/* set: nothing the blob stores is needed */
		err = handle_of(a1) ? ERR_NONE : ERR_BAD_BUFFER;
		break;
	case 18:		/* lock(buffer, usage, rect, fence, cb(Error, pointer)) */
		err = ERR_UNSUPPORTED;
		call_cb((const void *)a5, &err, &data, NULL);
		break;
	case 19:		/* unlock(buffer, cb(Error, hidl_handle)) */
		err = ERR_UNSUPPORTED;
		call_cb((const void *)a2, &err, &no_fence, NULL);
		break;
	default:
		err = ERR_UNSUPPORTED;
		break;
	}
	if (debug && slot != 23)
		fprintf(stderr, "hybris-gralloc: %s(%#llx) = %d\n", slot < 32 ? slot_name[slot] : "?",
			(unsigned long long)a1, err);
	ret_ok(sret, err);
	return 0;
}

/* every slot: pass the x8 result pointer and the slot number to slot_dispatch */
__asm__(
	".pushsection .text\n"
	".macro HYB_SLOT n\n"
	"	.global hyb_slot_\\n\n"
	"	.hidden hyb_slot_\\n\n"
	"hyb_slot_\\n:\n"
	"	mov x7, x8\n"
	"	mov x6, #\\n\n"
	"	b slot_dispatch\n"
	".endm\n"
	".irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31\n"
	"	HYB_SLOT \\n\n"
	".endr\n"
	".global hyb_get_service\n"
	".hidden hyb_get_service\n"
	"hyb_get_service:\n"
	"	mov x0, x8\n"
	"	b get_service_c\n"
	".popsection\n");

#define S(n) extern void hyb_slot_##n(void);
S(0) S(1) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9) S(10) S(11) S(12) S(13) S(14) S(15)
S(16) S(17) S(18) S(19) S(20) S(21) S(22) S(23) S(24) S(25) S(26) S(27) S(28) S(29) S(30) S(31)
extern void hyb_get_service(void);

/* IMapper::getService() returns sp<IMapper> through x8 */
HIDDEN void get_service_c(void **out)
{
	*out = &mapper;
	refbase_incstrong(&mapper.rb_vptr, out);
}

static void *hook(const char *sym, const char *requester)
{
	if (strstr(sym, "mapper4V4_07IMapper10getService")) {
		if (debug)
			fprintf(stderr, "hybris-gralloc: %s for %s\n", sym, requester);
		return hyb_get_service;
	}
	return NULL;
}

int hyb_gralloc_init(void)
{
	void *slots[32] = {
		hyb_slot_0, hyb_slot_1, hyb_slot_2, hyb_slot_3, hyb_slot_4, hyb_slot_5, hyb_slot_6,
		hyb_slot_7, hyb_slot_8, hyb_slot_9, hyb_slot_10, hyb_slot_11, hyb_slot_12, hyb_slot_13,
		hyb_slot_14, hyb_slot_15, hyb_slot_16, hyb_slot_17, hyb_slot_18, hyb_slot_19, hyb_slot_20,
		hyb_slot_21, hyb_slot_22, hyb_slot_23, hyb_slot_24, hyb_slot_25, hyb_slot_26, hyb_slot_27,
		hyb_slot_28, hyb_slot_29, hyb_slot_30, hyb_slot_31,
	};
	void *utils;

	debug = getenv("HYBRIS_GRALLOC_DEBUG") != NULL;
	android_dlopen = dlsym(RTLD_DEFAULT, "android_dlopen");
	android_dlsym = dlsym(RTLD_DEFAULT, "android_dlsym");
	set_hook = dlsym(RTLD_DEFAULT, "hybris_set_hook_callback");
	if (!android_dlopen || !android_dlsym || !set_hook)
		return -1;
	utils = android_dlopen("libutils.so", RTLD_NOW);
	if (!utils)
		return -1;
	string8_ctor = android_dlsym(utils, "_ZN7android7String8C1Ev");
	refbase_ctor = android_dlsym(utils, "_ZN7android7RefBaseC2Ev");
	refbase_incstrong = android_dlsym(utils, "_ZNK7android7RefBase9incStrongEPKv");
	if (!string8_ctor || !refbase_ctor || !refbase_incstrong)
		return -1;

	vtbl[0] = (void *)16;	/* offset of the RefBase virtual base */
	memcpy(&vtbl[3], slots, sizeof(slots));
	mapper.vptr = &vtbl[3];
	refbase_ctor(&mapper.rb_vptr);
	refbase_incstrong(&mapper.rb_vptr, &mapper);	/* never dropped */
	set_hook(hook);
	return 0;
}

static void buf_inc_ref(void *p)
{
	atomic_fetch_add(&((struct hyb_buffer *)p)->refs, 1);
}

static void buf_dec_ref(void *p)
{
	struct hyb_buffer *b = p;

	if (atomic_fetch_sub(&b->refs, 1) == 1) {
		close(b->handle.fd);
		free(b);
	}
}

struct hyb_buffer *hyb_buffer_wrap(int fd, int width, int height, uint32_t fourcc,
				   uint64_t modifier, uint32_t offset, uint32_t pitch)
{
	static atomic_uint ids;
	const struct hyb_format *f = hyb_format(fourcc);
	off_t size = lseek(fd, 0, SEEK_END);
	uint32_t want_pitch;
	struct hyb_buffer *b;

	if (modifier == HYB_MOD_INVALID)
		modifier = HYB_MOD_LINEAR;
	if (!hyb_supported(f, modifier) || offset || width <= 0 || height <= 0 || size <= 0)
		goto fail;
	if (hyb_is_afbc(modifier)) {
		if (size < (off_t)hyb_layout(width, height, f, modifier, &want_pitch))
			goto fail;
		pitch = want_pitch;
	} else if (pitch < (uint32_t)width * f->cpp || pitch % f->cpp || (off_t)pitch * height > size) {
		goto fail;
	}
	b = calloc(1, sizeof(*b));
	if (!b)
		goto fail;
	atomic_init(&b->refs, 1);
	b->fourcc = fourcc;
	b->pitch = pitch;
	b->modifier = modifier;
	b->handle = (struct hyb_handle){
		.version = 12, .num_fds = 1, .num_ints = HYB_NUM_INTS, .fd = fd,
		.magic = HYB_MAGIC, .width = width, .height = height, .stride = pitch,
		.fourcc = f->blob_fourcc, .format = f->hal,
		.mod_lo = modifier, .mod_hi = modifier >> 32,
		.usage_lo = 0xb00,	/* GPU texture + render target + composer */
		.size = size, .id = atomic_fetch_add(&ids, 1) + 1,
	};
	b->anw = (struct anw_buffer){
		.magic = 0x5f626672,	/* '_bfr' */
		.version = sizeof(struct anw_buffer),
		.inc_ref = buf_inc_ref, .dec_ref = buf_dec_ref,
		.width = width, .height = height, .stride = pitch / f->cpp, .format = f->hal,
		.usage_deprecated = 0xb00, .layer_count = 1, .handle = &b->handle, .usage = 0xb00,
	};
	return b;
fail:
	close(fd);
	return NULL;
}

struct hyb_buffer *hyb_buffer_alloc(int width, int height, uint32_t fourcc, uint64_t modifier)
{
	const struct hyb_format *f = hyb_format(fourcc);
	uint32_t pitch;
	int fd;

	if (!f)
		return NULL;
	fd = hyb_dmabuf_alloc(hyb_layout(width, height, f, modifier, &pitch));
	return fd < 0 ? NULL : hyb_buffer_wrap(fd, width, height, fourcc, modifier, 0, pitch);
}

void hyb_buffer_unref(struct hyb_buffer *b)
{
	if (b)
		buf_dec_ref(b);
}

void *hyb_buffer_native(struct hyb_buffer *b)
{
	return &b->anw;
}

int hyb_buffer_fd(const struct hyb_buffer *b)
{
	return b->handle.fd;
}

uint32_t hyb_buffer_format(const struct hyb_buffer *b)
{
	return b->fourcc;
}

uint32_t hyb_buffer_pitch(const struct hyb_buffer *b)
{
	return b->pitch;
}

uint64_t hyb_buffer_modifier(const struct hyb_buffer *b)
{
	return b->modifier;
}
