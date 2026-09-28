# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="scummvm-lr"
PKG_VERSION="fcbce3ae815269dacdc309092bc92ccc6d3e13bb"
PKG_SHA256="7e60fec38740f90bb987c79d8f8623faa48485373907d3a15a13b0b3b353a316"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/libretro/scummvm"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="ScummVM with libretro backend."
PKG_TOOLCHAIN="make"
PKG_BUILD_FLAGS="-lto"

PKG_MAKE_OPTS_TARGET="-C ../backends/platform/libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ../backends/platform/libretro/scummvm_libretro.so ${INSTALL}/usr/lib/libretro
}
