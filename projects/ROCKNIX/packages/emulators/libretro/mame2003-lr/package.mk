# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mame2003-lr"
PKG_VERSION="b9ac708f922386a0ae1f2379dbb31b60d7ec7b25"
PKG_SHA256="3f45adec0787afff205c28dd0c99ee4dab096fe5742991eddebc2144ebf4d373"
PKG_LICENSE="MAME"
PKG_SITE="https://github.com/libretro/mame2003-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="MAME - Multiple Arcade Machine Emulator"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mame2003_libretro.so ${INSTALL}/usr/lib/libretro
}
