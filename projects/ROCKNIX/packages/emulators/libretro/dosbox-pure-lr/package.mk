# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="dosbox-pure-lr"
PKG_VERSION="73e03aa145e0549ed4d5a20f8e65532714da33f5"
PKG_SHA256="0fbc441291c71ea2c568e7b59d6243aa9b2adf093bac15292f02b8ca09701fe2"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/schellingb/dosbox-pure"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A port of DOSBox to libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a dosbox_pure_libretro.so ${INSTALL}/usr/lib/libretro
}
