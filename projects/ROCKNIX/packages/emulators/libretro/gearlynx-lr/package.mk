# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="gearlynx-lr"
PKG_VERSION="87235571f67b28de8fc1dec7cf0a5a04e26b23ad"
PKG_SHA256="59116be381629fd3c72b230a5d051b2078a73c9409831cd7d37a37c376c96b18"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/drhelius/Gearlynx"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Gearlynx is a very accurate, cross-platform Atari Lynx emulator written in C++ that runs on Windows, macOS, Linux, BSD and RetroArch."
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C platforms/libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a platforms/libretro/gearlynx_libretro.so ${INSTALL}/usr/lib/libretro
}
