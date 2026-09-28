# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="beetle-psx-lr"
PKG_VERSION="1b88f130cbc402e237a4567b6ba591ef2eec0421"
PKG_SHA256="b5c180f7f9b0b9ea0ebc41c809993677007dd794166f85ebdbc11dd4ea65f7c5"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/beetle-psx-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Fork of Mednafen PSX"

if [ ! "${OPENGL}" = "no" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu libglvnd"
fi

if [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
fi

if [ "${VULKAN_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${VULKAN}"
fi

PKG_MAKE_OPTS_TARGET+=" LINK_STATIC_LIBCPLUSPLUS=0"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mednafen_psx_libretro.so ${INSTALL}/usr/lib/libretro/beetle_psx_libretro.so
}
