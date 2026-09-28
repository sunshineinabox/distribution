# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="primehack-sa"
PKG_VERSION="53f53e0f5bad27ad62a807cb93b136d84f68777f" # 1.0.9
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/shiiion/dolphin"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain libevdev libdrm ffmpeg zlib libpng lzo libusb zstd ecm openal-soft pulseaudio alsa-lib libfmt hidapi curl SDL3 qt6"
PKG_LONGDESC="PrimeHack is a Dolphin fork for the Metroid Prime Trilogy with twin-stick camera controls"

if [ "${OPENGL_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu libglvnd"
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_EGL=ON"
elif [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_EGL=ON"
fi

if [ "${DISPLAYSERVER}" = "wl" ]; then
  PKG_DEPENDS_TARGET+=" wayland wayland-protocols ${WINDOWMANAGER} xwayland xrandr libXi"
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_X11=ON \
                           -DENABLE_WAYLAND=ON"
else
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_X11=OFF \
                           -DENABLE_WAYLAND=OFF"
fi

if [ "${VULKAN_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${VULKAN}"
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_VULKAN=ON"
  GRENDERER="Vulkan"
else
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_VULKAN=OFF"
  GRENDERER="OGL"
fi

# Binaries and data are renamed so they can sit next to dolphin-sa
PKG_CMAKE_OPTS_TARGET+=" -DENABLE_QT=ON \
                         -DENABLE_NOGUI=ON \
                         -DUSE_RETRO_ACHIEVEMENTS=OFF \
                         -DENABLE_HEADLESS=OFF \
                         -DCMAKE_BUILD_TYPE=Release \
                         -DDISTRIBUTOR="ROCKNIX" \
                         -Ddatadir=/usr/share/primehack \
                         -DENABLE_EVDEV=ON \
                         -DENABLE_SDL=ON \
                         -DUSE_DISCORD_PRESENCE=OFF \
                         -DBUILD_SHARED_LIBS=OFF \
                         -DLINUX_LOCAL_DEV=OFF \
                         -DENABLE_PULSEAUDIO=ON \
                         -DENABLE_ALSA=ON \
                         -DENABLE_TESTS=OFF \
                         -DENABLE_LLVM=OFF \
                         -DENABLE_ANALYTICS=OFF \
                         -DENABLE_LTO=OFF \
                         -DENCODE_FRAMEDUMPS=OFF \
                         -DENABLE_AUTOUPDATE=OFF \
                         -DUSE_MGBA=OFF \
                         -DENABLE_CLI_TOOL=OFF \
                         -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

post_unpack() {
  sed -i 's~#include <cstdlib>~#include <cstdlib>\n#include <cstdint>~g' ${PKG_BUILD}/Externals/VulkanMemoryAllocator/include/vk_mem_alloc.h
  sed -i 's~#include <cstdint>~#include <cstdint>\n#include <string>~g' ${PKG_BUILD}/Externals/VulkanMemoryAllocator/include/vk_mem_alloc.h
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -a ${PKG_BUILD}/.${TARGET_NAME}/Binaries/dolphin-emu ${INSTALL}/usr/bin/primehack
    cp -a ${PKG_BUILD}/.${TARGET_NAME}/Binaries/dolphin-emu-nogui ${INSTALL}/usr/bin/primehack-nogui
    cp -a ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/usr/share/primehack
    cp -a ${PKG_BUILD}/Data/Sys ${INSTALL}/usr/share/primehack/sys

  mkdir -p ${INSTALL}/usr/config/primehack
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/primehack
}

post_install() {
  sed -e "s/@GRENDERER@/${GRENDERER}/g" -i ${INSTALL}/usr/bin/start_primehack.sh
}
