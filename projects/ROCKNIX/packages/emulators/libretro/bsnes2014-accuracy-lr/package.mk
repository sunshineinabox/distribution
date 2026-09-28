# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="bsnes2014-accuracy-lr"
PKG_VERSION="3c1394e042ee444c8248e1b9210e14ea55e836e9"
PKG_SHA256="817a9e33657f0513b17eb0f5fafd3f1cf96df28aef260ebfdc4559dc2e9782f4"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/libretro/bsnes2014"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Libretro fork of bsnes. Built for accuracy."

PKG_MAKE_OPTS_TARGET="PROFILE=accuracy"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a bsnes2014_accuracy_libretro.so ${INSTALL}/usr/lib/libretro
}
