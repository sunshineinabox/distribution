# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="beetle-pce-fast-lr"
PKG_VERSION="1c693c630121366f0b819d785762f1ce5b3a68d0"
PKG_SHA256="8b427af5f0dcb07067f69699eab5f37386fed757cf373216c62ea20e75fb1e02"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/beetle-pce-fast-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Standalone port of Mednafen PCE Fast to libretro."

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mednafen_pce_fast_libretro.so ${INSTALL}/usr/lib/libretro/beetle_pce_fast_libretro.so
}
