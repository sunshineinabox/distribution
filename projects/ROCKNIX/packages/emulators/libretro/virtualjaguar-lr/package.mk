# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="virtualjaguar-lr"
PKG_VERSION="f9a3c89f58836cb2c45a42ad4edfac047050f30a"
PKG_SHA256="2c330f1893824cb8616a8e0c1fa0686b064a7f7cba91e5db5d44e09e8db7807f"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://github.com/libretro/virtualjaguar-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of Virtual Jaguar to Libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a virtualjaguar_libretro.so ${INSTALL}/usr/lib/libretro
}
