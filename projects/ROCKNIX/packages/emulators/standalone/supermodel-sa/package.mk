# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="supermodel-sa"
PKG_VERSION="6ae0cf2f237586c4a3cc791514ec1b0f3cd4c56c"
PKG_SHA256="f29f12962da1f3ffaf8a08f00d089495e2a7fbc546416ca019250bdfcba051ca"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/DirtBagXon/model3emu-code-sinden"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="${OPENGL} ${OPENGLES} glu toolchain SDL2 SDL2_net zlib"
PKG_LONGDESC="Supermodel is a Sega Model 3 arcade emulator"

PKG_MAKE_OPTS="NET_BOARD=1"

post_unpack() {
  cp ${PKG_BUILD}/Makefiles/Makefile.UNIX ${PKG_BUILD}/Makefile
  sed -e "s+MUSASHI_CFLAGS =+MUSASHI_CFLAGS = -I${SYSROOT_PREFIX}/usr/include+g" -i ${PKG_BUILD}/Makefiles/Rules.inc
  sed -i "s|sdl2-config|${SYSROOT_PREFIX}/usr/bin/sdl2-config|g" ${PKG_BUILD}/Makefile
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -a ${PKG_BUILD}/bin/supermodel ${INSTALL}/usr/bin
    cp -a ${PKG_DIR}/scripts/start_supermodel.sh ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/usr/config/supermodel/Config
    cp -a ${PKG_BUILD}/Config/Games.xml ${INSTALL}/usr/config/supermodel/Config
    cp -a ${PKG_DIR}/config/${DEVICE}/* ${INSTALL}/usr/config/supermodel/Config
}
