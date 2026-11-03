// SPDX-License-Identifier: Apache-2.0
// libvulkan.so.1 for gpudriver libmali: libhybris' Vulkan with Wayland and headless surfaces.
// Each surface is a hybris-window ANativeWindow handed to the blob as an Android one, so its
// swapchain renders straight into gralloc buffers presented through linux-dmabuf.
#define _GNU_SOURCE
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>
#include "hybris-gralloc.h"
#include "hybris-window.h"
#include "linux-dmabuf-v1-client-protocol.h"

#define EXPORT __attribute__((visibility("default")))
#define HIDDEN __attribute__((visibility("hidden"), used))
#define HYBRIS_VULKAN "/usr/lib/libhybris/libvulkan.so.1"

/* Every entry point not handled here jumps straight to libhybris' (vk-exports.h, from its
 * symbols); they can only be reached through an instance, so after load() */
#define X(name) HIDDEN void *real_##name;
#include "vk-exports.h"
#undef X
#define X(name) __asm__(".pushsection .text\n.globl " #name "\n.type " #name ", %function\n" \
			#name ":\n\tadrp x16, real_" #name "\n\tldr x16, [x16, :lo12:real_" #name "]\n" \
			"\tbr x16\n.popsection");
#include "vk-exports.h"
#undef X

struct wl_ctx {
	struct wl_display *display;
	struct wl_event_queue *queue;
	struct wl_display *wrapper;
	struct wl_registry *registry;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	struct hyb_wl_format fmt[HYB_NFORMATS];
	int surfaces;
	struct wl_ctx *next;
};

struct surf {
	VkSurfaceKHR handle;
	struct hyb_window *window;
	struct wl_ctx *ctx;		/* NULL headless */
	struct surf *next;
};

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void *hybris;
static PFN_vkGetInstanceProcAddr hybris_gipa;
static PFN_vkCreateInstance hybris_create_instance;
static PFN_vkEnumerateInstanceExtensionProperties hybris_instance_exts;
static PFN_vkEnumerateInstanceLayerProperties hybris_instance_layers;
static PFN_vkEnumerateInstanceVersion hybris_instance_version;
static PFN_vkGetDeviceProcAddr hybris_gdpa;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR hybris_surface_caps;
static PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR hybris_surface_caps2;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR hybris_surface_formats;
static PFN_vkGetPhysicalDeviceSurfaceFormats2KHR hybris_surface_formats2;
static struct wl_ctx *ctxs;
static struct surf *surfs;
static int sync_files, stats;

static void load(void)
{
	const char *sync = getenv("WLR_GLES2_SYNC_FILES");

	stats = getenv("HYBRIS_SHIM_STATS") != NULL;
	/* the compositor waits on dma-buf fences (wlroots 0010) */
	sync_files = sync && !strcmp(sync, "1");
	/* the mapper and thread hooks first: libhybris loads the blob on the first call */
	hyb_gralloc_init();
	/* these surfaces stand in for libhybris' window systems */
	setenv("HYBRIS_VULKANPLATFORM", "null", 1);
	/* deep: libhybris calls its own vk* entry points, not the shim's of the same names */
	hybris = dlopen(HYBRIS_VULKAN, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	if (!hybris) {
		fprintf(stderr, "hybris-vulkan: %s\n", dlerror());
		return;
	}
#define X(name) real_##name = dlsym(hybris, #name);
#include "vk-exports.h"
#undef X
	hybris_gipa = (PFN_vkGetInstanceProcAddr)dlsym(hybris, "vkGetInstanceProcAddr");
	hybris_create_instance = (PFN_vkCreateInstance)dlsym(hybris, "vkCreateInstance");
	hybris_instance_exts = (PFN_vkEnumerateInstanceExtensionProperties)
		dlsym(hybris, "vkEnumerateInstanceExtensionProperties");
	hybris_instance_layers = (PFN_vkEnumerateInstanceLayerProperties)
		dlsym(hybris, "vkEnumerateInstanceLayerProperties");
	hybris_instance_version = (PFN_vkEnumerateInstanceVersion)dlsym(hybris, "vkEnumerateInstanceVersion");
	hybris_gdpa = (PFN_vkGetDeviceProcAddr)dlsym(hybris, "vkGetDeviceProcAddr");
	hybris_surface_caps = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
		dlsym(hybris, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	hybris_surface_caps2 = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
		dlsym(hybris, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
	hybris_surface_formats = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)
		dlsym(hybris, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	hybris_surface_formats2 = (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)
		dlsym(hybris, "vkGetPhysicalDeviceSurfaceFormats2KHR");
}

static void init(void)
{
	pthread_once(&once, load);
}

EXPORT VkResult vkEnumerateInstanceVersion(uint32_t *version)
{
	init();
	if (hybris_instance_version)
		return hybris_instance_version(version);
	*version = VK_API_VERSION_1_1;
	return VK_SUCCESS;
}

EXPORT VkResult vkEnumerateInstanceLayerProperties(uint32_t *count, VkLayerProperties *props)
{
	init();
	return hybris_instance_layers ? hybris_instance_layers(count, props) : VK_ERROR_INITIALIZATION_FAILED;
}

static int is_surface_ext(const char *name)
{
	return !strcmp(name, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) ||
	       !strcmp(name, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) ||
	       !strcmp(name, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
}

/* Android surfaces stay internal: apps get Wayland and headless ones */
EXPORT VkResult vkEnumerateInstanceExtensionProperties(const char *layer, uint32_t *count,
						       VkExtensionProperties *props)
{
	static const VkExtensionProperties own[] = {
		{ VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_SPEC_VERSION },
		{ VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_SPEC_VERSION },
	};
	VkExtensionProperties *all;
	uint32_t n = 0, m = 0;
	VkResult r;

	init();
	if (!hybris_instance_exts)
		return VK_ERROR_INITIALIZATION_FAILED;
	if (layer)
		return hybris_instance_exts(layer, count, props);
	r = hybris_instance_exts(NULL, &n, NULL);
	if (r != VK_SUCCESS)
		return r;
	all = calloc(n + 2, sizeof(*all));
	if (!all)
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	r = hybris_instance_exts(NULL, &n, all);
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		free(all);
		return r;
	}
	for (uint32_t i = 0; i < n; i++)
		if (!is_surface_ext(all[i].extensionName))
			all[m++] = all[i];
	memcpy(&all[m], own, sizeof(own));
	m += 2;
	r = VK_SUCCESS;
	if (props) {
		if (*count < m) {
			m = *count;
			r = VK_INCOMPLETE;
		}
		memcpy(props, all, m * sizeof(*all));
	}
	*count = m;
	free(all);
	return r;
}

EXPORT VkResult vkCreateInstance(const VkInstanceCreateInfo *info, const VkAllocationCallbacks *alloc,
				 VkInstance *instance)
{
	VkInstanceCreateInfo ci = *info;
	const char **names;
	uint32_t n = 0;
	int android = 0;
	VkResult r;

	init();
	if (!hybris_create_instance)
		return VK_ERROR_INITIALIZATION_FAILED;
	names = calloc(info->enabledExtensionCount + 1, sizeof(*names));
	if (!names)
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	for (uint32_t i = 0; i < info->enabledExtensionCount; i++) {
		const char *e = info->ppEnabledExtensionNames[i];

		if (is_surface_ext(e)) {
			if (android++)
				continue;
			e = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
		}
		names[n++] = e;
	}
	ci.enabledExtensionCount = n;
	ci.ppEnabledExtensionNames = names;
	r = hybris_create_instance(&ci, alloc, instance);
	free(names);
	return r;
}

static void reg_global(void *data, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver)
{
	struct wl_ctx *c = data;

	if (!strcmp(iface, zwp_linux_dmabuf_v1_interface.name) && !c->dmabuf && ver >= 3) {
		/* version 3 lists the modifiers itself */
		c->dmabuf = wl_registry_bind(r, name, &zwp_linux_dmabuf_v1_interface, 3);
		hyb_window_listen_dmabuf(c->dmabuf, c->fmt);
	}
}
static void reg_remove(void *data, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

/* The compositor's linux-dmabuf and its formats, per connection with surfaces (under lock) */
static struct wl_ctx *ctx_get(struct wl_display *display)
{
	struct wl_ctx *c;

	for (c = ctxs; c && c->display != display; c = c->next)
		;
	if (c || !(c = calloc(1, sizeof(*c))))
		return c;
	c->display = display;
	c->queue = wl_display_create_queue(display);
	c->wrapper = wl_proxy_create_wrapper(display);
	wl_proxy_set_queue((struct wl_proxy *)c->wrapper, c->queue);
	c->registry = wl_display_get_registry(c->wrapper);
	wl_registry_add_listener(c->registry, &reg_listener, c);
	wl_display_roundtrip_queue(display, c->queue);
	wl_display_roundtrip_queue(display, c->queue);
	c->next = ctxs;
	ctxs = c;
	return c;
}

static void ctx_put(struct wl_ctx *c)
{
	struct wl_ctx **pp;

	if (--c->surfaces > 0)
		return;
	for (pp = &ctxs; *pp != c; pp = &(*pp)->next)
		;
	*pp = c->next;
	if (c->dmabuf)
		zwp_linux_dmabuf_v1_destroy(c->dmabuf);
	wl_registry_destroy(c->registry);
	wl_proxy_wrapper_destroy(c->wrapper);
	wl_event_queue_destroy(c->queue);
	free(c);
}

/* An Android surface over a hybris-window on the wl_surface, or headless without one */
static VkResult make_surface(VkInstance instance, const VkAllocationCallbacks *alloc, VkSurfaceKHR *out,
			     struct wl_display *display, struct wl_surface *surface)
{
	PFN_vkCreateAndroidSurfaceKHR create = hybris_gipa ?
		(PFN_vkCreateAndroidSurfaceKHR)hybris_gipa(instance, "vkCreateAndroidSurfaceKHR") : NULL;
	struct hyb_window_config cfg = {
		.display = display, .surface = surface, .sync_files = sync_files, .stats = stats,
	};
	VkAndroidSurfaceCreateInfoKHR info = { .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
	VkResult r = VK_ERROR_INITIALIZATION_FAILED;
	struct surf *s;

	if (!create)
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	if (!(s = calloc(1, sizeof(*s))))
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	pthread_mutex_lock(&lock);
	if (display && (s->ctx = ctx_get(display))) {
		s->ctx->surfaces++;
		cfg.dmabuf = s->ctx->dmabuf;
		cfg.formats = s->ctx->fmt;
	}
	pthread_mutex_unlock(&lock);
	/* presented as linux-dmabuf buffers, in a format the compositor takes */
	if ((!display || (cfg.dmabuf && s->ctx->fmt[0].supported)) && (s->window = hyb_window_create(&cfg))) {
		info.window = hyb_window_native(s->window);
		r = create(instance, &info, alloc, out);
	}
	if (r != VK_SUCCESS) {
		if (s->window) {
			hyb_window_detach(s->window);
			hyb_window_unref(s->window);
		}
		pthread_mutex_lock(&lock);
		if (s->ctx)
			ctx_put(s->ctx);
		pthread_mutex_unlock(&lock);
		free(s);
		return r;
	}
	s->handle = *out;
	pthread_mutex_lock(&lock);
	s->next = surfs;
	surfs = s;
	pthread_mutex_unlock(&lock);
	return VK_SUCCESS;
}

EXPORT VkResult vkCreateWaylandSurfaceKHR(VkInstance instance, const VkWaylandSurfaceCreateInfoKHR *info,
					  const VkAllocationCallbacks *alloc, VkSurfaceKHR *surface)
{
	return make_surface(instance, alloc, surface, info->display, info->surface);
}

EXPORT VkResult vkCreateHeadlessSurfaceEXT(VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT *info,
					   const VkAllocationCallbacks *alloc, VkSurfaceKHR *surface)
{
	return make_surface(instance, alloc, surface, NULL, NULL);
}

EXPORT VkBool32 vkGetPhysicalDeviceWaylandPresentationSupportKHR(VkPhysicalDevice pd, uint32_t family,
								 struct wl_display *display)
{
	return VK_TRUE;
}

/* Wayland buffers are opaque or premultiplied rather than Android's INHERIT, and mailbox
 * needs four images to run ahead of the two the display holds, as in Mesa */
static void wayland_caps(VkSurfaceCapabilitiesKHR *caps)
{
	caps->supportedCompositeAlpha |= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	if (caps->minImageCount < 4 && caps->maxImageCount >= 4)
		caps->minImageCount = 4;
}

EXPORT VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice pd, VkSurfaceKHR surface,
							  VkSurfaceCapabilitiesKHR *caps)
{
	VkResult r = hybris_surface_caps ? hybris_surface_caps(pd, surface, caps) : VK_ERROR_INITIALIZATION_FAILED;

	if (r == VK_SUCCESS)
		wayland_caps(caps);
	return r;
}

EXPORT VkResult vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice pd,
							   const VkPhysicalDeviceSurfaceInfo2KHR *info,
							   VkSurfaceCapabilities2KHR *caps)
{
	VkResult r = hybris_surface_caps2 ? hybris_surface_caps2(pd, info, caps) : VK_ERROR_INITIALIZATION_FAILED;

	if (r == VK_SUCCESS)
		wayland_caps(&caps->surfaceCapabilities);
	return r;
}

/* Android also offers half-float and 10-bit swapchains, which the buffers and compositor do not take */
static int our_format(VkFormat f)
{
	return f == VK_FORMAT_R8G8B8A8_UNORM || f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_R5G6B5_UNORM_PACK16;
}

/* *count and props from the m entries kept, the Vulkan way */
static VkResult count_out(uint32_t m, uint32_t *count, int fill, uint32_t *copy)
{
	VkResult r = VK_SUCCESS;

	if (fill && *count < m) {
		m = *count;
		r = VK_INCOMPLETE;
	}
	*copy = fill ? m : 0;
	*count = m;
	return r;
}

EXPORT VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice pd, VkSurfaceKHR surface,
						     uint32_t *count, VkSurfaceFormatKHR *props)
{
	VkSurfaceFormatKHR *all;
	uint32_t n = 0, m = 0, copy;
	VkResult r;

	if (!hybris_surface_formats)
		return VK_ERROR_INITIALIZATION_FAILED;
	r = hybris_surface_formats(pd, surface, &n, NULL);
	if (r != VK_SUCCESS)
		return r;
	if (!(all = calloc(n ? n : 1, sizeof(*all))))
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	r = hybris_surface_formats(pd, surface, &n, all);
	if (r == VK_SUCCESS || r == VK_INCOMPLETE) {
		for (uint32_t i = 0; i < n; i++)
			if (our_format(all[i].format))
				all[m++] = all[i];
		r = count_out(m, count, props != NULL, &copy);
		if (copy)
			memcpy(props, all, copy * sizeof(*all));
	}
	free(all);
	return r;
}

EXPORT VkResult vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice pd,
						      const VkPhysicalDeviceSurfaceInfo2KHR *info,
						      uint32_t *count, VkSurfaceFormat2KHR *props)
{
	VkSurfaceFormat2KHR *all;
	uint32_t n = 0, m = 0, copy;
	VkResult r;

	if (!hybris_surface_formats2)
		return VK_ERROR_INITIALIZATION_FAILED;
	r = hybris_surface_formats2(pd, info, &n, NULL);
	if (r != VK_SUCCESS)
		return r;
	if (!(all = calloc(n ? n : 1, sizeof(*all))))
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	for (uint32_t i = 0; i < n; i++)
		all[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
	r = hybris_surface_formats2(pd, info, &n, all);
	if (r == VK_SUCCESS || r == VK_INCOMPLETE) {
		for (uint32_t i = 0; i < n; i++)
			if (our_format(all[i].surfaceFormat.format))
				all[m++] = all[i];
		r = count_out(m, count, props != NULL, &copy);
		/* the app's sType and pNext stay */
		for (uint32_t i = 0; i < copy; i++)
			props[i].surfaceFormat = all[i].surfaceFormat;
	}
	free(all);
	return r;
}

EXPORT VkResult vkCreateSwapchainKHR(VkDevice dev, const VkSwapchainCreateInfoKHR *info,
				     const VkAllocationCallbacks *alloc, VkSwapchainKHR *swapchain)
{
	PFN_vkCreateSwapchainKHR create = hybris_gdpa ?
		(PFN_vkCreateSwapchainKHR)hybris_gdpa(dev, "vkCreateSwapchainKHR") : NULL;
	struct surf *s;

	if (!create)
		return VK_ERROR_INITIALIZATION_FAILED;
	pthread_mutex_lock(&lock);
	for (s = surfs; s && s->handle != info->surface; s = s->next)
		;
	if (s)
		hyb_window_set_opaque(s->window, info->compositeAlpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
	pthread_mutex_unlock(&lock);
	return create(dev, info, alloc, swapchain);
}

EXPORT PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice dev, const char *name)
{
	if (!strcmp(name, "vkCreateSwapchainKHR"))
		return (PFN_vkVoidFunction)vkCreateSwapchainKHR;
	return hybris_gdpa ? hybris_gdpa(dev, name) : NULL;
}

EXPORT void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *alloc)
{
	PFN_vkDestroySurfaceKHR destroy = hybris_gipa ?
		(PFN_vkDestroySurfaceKHR)hybris_gipa(instance, "vkDestroySurfaceKHR") : NULL;
	struct surf *s, **pp;

	if (surface == VK_NULL_HANDLE)
		return;
	if (destroy)
		destroy(instance, surface, alloc);
	pthread_mutex_lock(&lock);
	for (pp = &surfs; (s = *pp) && s->handle != surface; pp = &s->next)
		;
	if (s)
		*pp = s->next;
	pthread_mutex_unlock(&lock);
	if (!s)
		return;
	hyb_window_detach(s->window);
	hyb_window_unref(s->window);
	pthread_mutex_lock(&lock);
	if (s->ctx)
		ctx_put(s->ctx);
	pthread_mutex_unlock(&lock);
	free(s);
}

EXPORT PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
	static const struct {
		const char *name;
		PFN_vkVoidFunction fn;
	} own[] = {
		{ "vkCreateHeadlessSurfaceEXT", (PFN_vkVoidFunction)vkCreateHeadlessSurfaceEXT },
		{ "vkCreateInstance", (PFN_vkVoidFunction)vkCreateInstance },
		{ "vkCreateSwapchainKHR", (PFN_vkVoidFunction)vkCreateSwapchainKHR },
		{ "vkCreateWaylandSurfaceKHR", (PFN_vkVoidFunction)vkCreateWaylandSurfaceKHR },
		{ "vkDestroySurfaceKHR", (PFN_vkVoidFunction)vkDestroySurfaceKHR },
		{ "vkEnumerateInstanceExtensionProperties", (PFN_vkVoidFunction)vkEnumerateInstanceExtensionProperties },
		{ "vkEnumerateInstanceLayerProperties", (PFN_vkVoidFunction)vkEnumerateInstanceLayerProperties },
		{ "vkEnumerateInstanceVersion", (PFN_vkVoidFunction)vkEnumerateInstanceVersion },
		{ "vkGetDeviceProcAddr", (PFN_vkVoidFunction)vkGetDeviceProcAddr },
		{ "vkGetInstanceProcAddr", (PFN_vkVoidFunction)vkGetInstanceProcAddr },
		{ "vkGetPhysicalDeviceSurfaceCapabilities2KHR",
		  (PFN_vkVoidFunction)vkGetPhysicalDeviceSurfaceCapabilities2KHR },
		{ "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
		  (PFN_vkVoidFunction)vkGetPhysicalDeviceSurfaceCapabilitiesKHR },
		{ "vkGetPhysicalDeviceSurfaceFormats2KHR", (PFN_vkVoidFunction)vkGetPhysicalDeviceSurfaceFormats2KHR },
		{ "vkGetPhysicalDeviceSurfaceFormatsKHR", (PFN_vkVoidFunction)vkGetPhysicalDeviceSurfaceFormatsKHR },
		{ "vkGetPhysicalDeviceWaylandPresentationSupportKHR",
		  (PFN_vkVoidFunction)vkGetPhysicalDeviceWaylandPresentationSupportKHR },
	};

	init();
	for (size_t i = 0; i < sizeof(own) / sizeof(own[0]); i++)
		if (!strcmp(name, own[i].name))
			return own[i].fn;
	return hybris_gipa ? hybris_gipa(instance, name) : NULL;
}
