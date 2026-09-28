# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="snes9x2002-lr"
PKG_VERSION="6ffbf9ef4f0063e1f1b78a40d10c50fc52f2524c"
PKG_SHA256="01c733054f8d02b26c53ab3f89ae0698e1f4edb55999dfa63c9a522fccb6382d"
PKG_LICENSE="Non-commercial"
PKG_SITE="https://github.com/libretro/snes9x2002"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Snes9x 2002."

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a snes9x2002_libretro.so ${INSTALL}/usr/lib/libretro
}
