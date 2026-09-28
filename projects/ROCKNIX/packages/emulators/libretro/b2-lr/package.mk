# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="b2-lr"
PKG_VERSION="9793f6a8040c416d147003cada29363b955b54e7"
PKG_SHA256="c8c2d1061fc76275c2dd4950ca4b692a3c1659ffadb9f1585c741deaabac64a0"
PKG_LICENSE="GPL-3.0-only"
PKG_SITE="https://github.com/zoltanvb/b2-libretro"
PKG_URL="https://github.com/zoltanvb/b2-libretro/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Adaptation of Tom Seddon's b2 emulator for BBC Micro"
PKG_TOOLCHAIN="make"

make_target() {
  cd ${PKG_BUILD}/src/libretro
  make -j$(getconf _NPROCESSORS_ONLN) clean
  make -j$(getconf _NPROCESSORS_ONLN)
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a b2_libretro.so ${INSTALL}/usr/lib/libretro
}
