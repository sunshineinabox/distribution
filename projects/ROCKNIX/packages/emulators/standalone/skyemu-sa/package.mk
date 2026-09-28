# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="skyemu-sa"
PKG_VERSION="01516d6798e3652b583e6a366085bb51c43b528d"
PKG_SHA256="479071a294080a746efac57e50dc1e43506bbabeacd133b924452995a08a3b21"
PKG_LICENSE="MIT"
PKG_SITE="https://github.com/skylersaleh/SkyEmu"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_LONGDESC="SkyEmu is a low level GameBoy, GameBoy Color, Game Boy Advance, and DS emulator."
PKG_DEPENDS_TARGET="toolchain SDL2 openssl curl"

PKG_CMAKE_OPTS_TARGET+=" -DCMAKE_BUILD_TYPE=Release \
                         -DENABLE_RETRO_ACHIEVEMENTS=ON \
                         -DUSE_SYSTEM_CURL=ON \
                         -DUSE_SYSTEM_OPENSSL=ON \
                         -DUSE_SYSTEM_SDL2=ON"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -a ${PKG_BUILD}/.${TARGET_NAME}/bin/SkyEmu ${INSTALL}/usr/bin
    cp -a ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/usr/config/SkyEmu
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/SkyEmu
}
