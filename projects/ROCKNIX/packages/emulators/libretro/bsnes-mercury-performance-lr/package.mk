# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="bsnes-mercury-performance-lr"
PKG_VERSION="79d7f9de218b6ffa65a80bbdc5828532bc239232"
PKG_SHA256="605b74dce8dd61499313b368fe411142b81bf3afe05d6e8197ebb8cba1ff4c9c"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/libretro/bsnes-mercury"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="BSNES Super Nintendo Libretro Core"

PKG_MAKE_OPTS_TARGET="platform=unix PROFILE=performance"

post_unpack() {
  sed -i 's/\-O[23]/-Ofast/' ${PKG_BUILD}/Makefile
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a bsnes_mercury_performance_libretro.so ${INSTALL}/usr/lib/libretro
}

