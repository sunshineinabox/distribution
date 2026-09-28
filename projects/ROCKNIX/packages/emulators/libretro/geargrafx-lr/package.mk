# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="geargrafx-lr"
PKG_VERSION="fc5f29f53c46ed59713ebd0d3ca3b4bac141d986"
PKG_SHA256="bc16b5bd68752f2c82bebd8a39d54d3d7524ad1508143fb00e455c71f0096d84"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/drhelius/Geargrafx"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Geargrafx is a very accurate, cross-platform TurboGrafx-16 / PC Engine / SuperGrafx / PCE CD-ROM² emulator written in C++ that runs on Windows, macOS, Linux, BSD and RetroArch."
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C platforms/libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a platforms/libretro/geargrafx_libretro.so ${INSTALL}/usr/lib/libretro
}
