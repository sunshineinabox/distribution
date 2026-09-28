# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="fuse-lr"
PKG_VERSION="e997e2bc32c888348f862f69f2c53babfedf7791"
PKG_SHA256="3b8af49fda13ed29ac6b9b03038a2761e397cc06a004dc499be64d5d3917d4a0"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/libretro/fuse-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A port of the Fuse Unix Spectrum Emulator to libretro "

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a fuse_libretro.so ${INSTALL}/usr/lib/libretro
}
