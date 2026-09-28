# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="beetle-pce-lr"
PKG_VERSION="b96c11e095b6a40a412d2da02766ae1c3f4fd539"
PKG_SHA256="dafa9d71ba2c6cc81c67145d7ac31d8ee32dd6fe8e9448335ae1bcb30b4a1205"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/beetle-pce-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Standalone port of Mednafen PCE to libretro."

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mednafen_pce_libretro.so ${INSTALL}/usr/lib/libretro/beetle_pce_libretro.so
}
