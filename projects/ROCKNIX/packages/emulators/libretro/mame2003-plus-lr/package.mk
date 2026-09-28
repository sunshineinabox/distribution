# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mame2003-plus-lr"
PKG_VERSION="31419303cbcbe2104a069f98948324963a226a4d"
PKG_SHA256="8155bc2a95be518a9e1fe76f781b2e358d75c414310267aaafaed650a1cda7f3"
PKG_LICENSE="MAME"
PKG_SITE="https://github.com/libretro/mame2003-plus-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="MAME - Multiple Arcade Machine Emulator"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mame2003_plus_libretro.so ${INSTALL}/usr/lib/libretro
}
