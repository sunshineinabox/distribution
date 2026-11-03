# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="hybris-mali"
PKG_VERSION=""
PKG_LICENSE="Apache-2.0"
PKG_SITE=""
PKG_URL=""
PKG_DEPENDS_TARGET="toolchain android-headers libhybris wayland wayland-protocols vulkan-headers mesa libdrm gpudriver"
PKG_LONGDESC="gpudriver libmali: the stock Mali blob of the device's own Android system through libhybris"
PKG_TOOLCHAIN="manual"

make_target() {
  local DMABUF_XML=${SYSROOT_PREFIX}/usr/share/wayland-protocols/stable/linux-dmabuf/linux-dmabuf-v1.xml
  local OWN_VK="vkCreateHeadlessSurfaceEXT|vkCreateInstance|vkCreateWaylandSurfaceKHR|vkDestroySurfaceKHR"
  OWN_VK+="|vkEnumerateInstanceExtensionProperties|vkEnumerateInstanceLayerProperties"
  OWN_VK+="|vkEnumerateInstanceVersion|vkGetInstanceProcAddr|vkGetPhysicalDeviceWaylandPresentationSupportKHR"
  OWN_VK+="|vkCreateSwapchainKHR|vkGetDeviceProcAddr|vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
  OWN_VK+="|vkGetPhysicalDeviceSurfaceCapabilities2KHR|vkGetPhysicalDeviceSurfaceFormatsKHR"
  OWN_VK+="|vkGetPhysicalDeviceSurfaceFormats2KHR"

  ${TOOLCHAIN}/bin/wayland-scanner client-header ${DMABUF_XML} linux-dmabuf-v1-client-protocol.h
  ${TOOLCHAIN}/bin/wayland-scanner private-code ${DMABUF_XML} linux-dmabuf-v1-protocol.c
  ${TARGET_PREFIX}nm -D --defined-only ${SYSROOT_PREFIX}/usr/lib/libhybris/libvulkan.so.1 | \
    awk -v own="^(${OWN_VK})\$" '$2 ~ /^[TWi]$/ && $3 ~ /^vk/ && $3 !~ own { print "X(" $3 ")" }' > vk-exports.h
  ${CC} ${CFLAGS} -fPIC -shared -fvisibility=hidden -Wl,-soname,libEGL.so.1 -Wl,-z,nodelete \
    -I. -I${SYSROOT_PREFIX}/usr/include/android \
    -o libEGL.so.1 egl-wayland-shim.c hybris-gralloc.c hybris-window.c linux-dmabuf-v1-protocol.c \
    ${LDFLAGS} -lwayland-client -ldl -lpthread
  ${CC} ${CFLAGS} -fPIC -shared -fvisibility=hidden -Wl,-soname,libvulkan.so.1 -Wl,-z,nodelete \
    -I. -I${SYSROOT_PREFIX}/usr/include/android \
    -o libvulkan.so.1 hybris-vulkan.c hybris-gralloc.c hybris-window.c linux-dmabuf-v1-protocol.c \
    ${LDFLAGS} -lwayland-client -ldl -lpthread
  ${CC} ${CFLAGS} -fPIC -shared -fvisibility=hidden -Wl,-soname,libgbm.so.1 \
    -o libgbm.so.1 gbm.c -I${SYSROOT_PREFIX}/usr/include/libdrm ${LDFLAGS} -ldrm
  ${CC} ${CFLAGS} -fPIC -shared -o libhybris-tls-pad.so tls-pad.c ${LDFLAGS}
  ${CC} ${CFLAGS} -o egl-check egl-check.c ${LDFLAGS} -lEGL -lGLESv2
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/hybris-mali
    cp libEGL.so.1 libgbm.so.1 libvulkan.so.1 libhybris-tls-pad.so egl-check android-mount env ${INSTALL}/usr/lib/hybris-mali

  mkdir -p ${INSTALL}/usr/lib/gpudriver
    cp gpudriver-libmali ${INSTALL}/usr/lib/gpudriver/libmali

  mkdir -p ${INSTALL}/vendor ${INSTALL}/apex ${INSTALL}/linkerconfig
    touch ${INSTALL}/vendor/.hybris-mali ${INSTALL}/apex/.hybris-mali
    cp ld.config.txt ${INSTALL}/linkerconfig
}
