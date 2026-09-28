# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="desmume-lr"
PKG_VERSION="95b4d798731caa809125b6c3c11d17cc332ff6ef"
PKG_SHA256="ecb657859366d5498e151cb804f19f9b0f2646fb8a4d7df21cc51cca0feca0f5"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/desmume"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain libpcap"
PKG_LONGDESC="DeSmuME - Nintendo DS libretro"
PKG_TOOLCHAIN="make"

if [ "${OPENGL_SUPPORT}" = "yes" ] && [ ! "${PREFER_GLES}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu libglvnd"
elif [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
  PKG_PATCH_DIRS+=" gles"
fi

make_target() {
  if [ "${ARCH}" = "arm" ]; then
    make -C desmume/src/frontend/libretro platform=armv-unix-${TARGET_FLOAT}float-${TARGET_CPU}
  elif [ "${ARCH}" = "x86_64" ]; then
    make -C desmume/src/frontend/libretro platform=unix
  else
    :
  fi
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  if [ "${ARCH}" = "aarch64" ]; then
    cp -a ${ROOT}/build.${DISTRO}-${DEVICE}.arm/install_pkg/desmume-*/usr/lib/libretro/desmume_libretro.so ${INSTALL}/usr/lib/libretro
  else
    cp -a desmume/src/frontend/libretro/desmume_libretro.so ${INSTALL}/usr/lib/libretro
  fi
}
