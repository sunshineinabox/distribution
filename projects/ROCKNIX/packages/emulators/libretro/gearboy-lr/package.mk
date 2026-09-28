# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="gearboy-lr"
PKG_VERSION="31dd84894dfbd8239f08a768805b7b3b12e61cf9"
PKG_SHA256="f1b8233605ae99d58df67eb0821083f831580419f1a533cbded47631e969b21e"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/drhelius/Gearboy"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Game Boy / Gameboy Color emulator for iOS, Mac, Raspberry Pi, Windows and Linux"
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C platforms/libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a platforms/libretro/gearboy_libretro.so ${INSTALL}/usr/lib/libretro
}
