# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="geolith-lr"
PKG_VERSION="194024931935eff2092e36fc4f8e53e62ed11097"
PKG_SHA256="3cf408638723be5d017a059f2e262c5e3f76d44f4b35cca894e85877cdf16bfa"
PKG_LICENSE="BSD-3-Clause"
PKG_SITE="https://github.com/libretro/geolith-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Geolith is a highly accurate emulator for the Neo Geo AES, MVS, CD, and CDZ."
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C libretro platform=${DEVICE}"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a libretro/geolith_libretro.so ${INSTALL}/usr/lib/libretro
}
