# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="hatari-lr"
PKG_VERSION="ab55c3ed0e620c91e7f059a6d3fbef7acf9bfca8"
PKG_SHA256="650e6dbbb2ec2a069f83198df77cf793923dbd91ff9653563d20d7be6039c412"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/hatari"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain capsimg"
PKG_LONGDESC="New rebasing of Hatari based on Mercurial upstream. Tries to be a shallow fork for easy upstreaming later on."
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C .. -f Makefile.libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ../hatari_libretro.so ${INSTALL}/usr/lib/libretro

  mkdir -p ${INSTALL}/usr/config/game/configs/hatari
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/game/configs/hatari/
}
