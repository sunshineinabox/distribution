# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="bk-lr"
PKG_VERSION="92042a753289cedf05e7e6bd8e75849f83e48966"
PKG_SHA256="5b04ce97b9e15eb3a773d14b1ed08c6c2905eaf8629144cad01e933528e94c06"
PKG_LICENSE="HPND"
PKG_SITE="https://github.com/libretro/bk-emulator"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Linux/SDL emulator for Soviet (russian) Electronica BK serie"

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ${PKG_BUILD}/bk_libretro.so ${INSTALL}/usr/lib/libretro
}
