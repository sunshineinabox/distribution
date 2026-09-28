# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="fmsx-lr"
PKG_VERSION="4de11755ce4f196ac1c8a7bb20bb4eccbc87a7d4"
PKG_SHA256="00d67d2bd41254d48fac9e9ae8c3b399aead46b3bcecca2e9f3cdf7f03feef44"
PKG_LICENSE="Non-commercial"
PKG_SITE="https://github.com/libretro/fmsx-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of fMSX 4.9 to the libretro API."

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a fmsx_libretro.so ${INSTALL}/usr/lib/libretro
}
