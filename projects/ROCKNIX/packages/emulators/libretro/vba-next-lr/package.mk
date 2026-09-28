# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vba-next-lr"
PKG_VERSION="788192f215ad0a1413f1625b40ebba3423fa0ade"
PKG_SHA256="e97c5839474183ba4b05db212b6c7b09ef7e0bfba3a2cab2ed8ebe34abe8339d"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/vba-next"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Optimized port of VBA-M to Libretro."

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a vba_next_libretro.so ${INSTALL}/usr/lib/libretro
}
