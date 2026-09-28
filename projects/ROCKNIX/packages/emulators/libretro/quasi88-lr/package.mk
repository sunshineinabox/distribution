# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="quasi88-lr"
PKG_VERSION="d43ef693eb93b65a1564229c05b32567cde1ae4e"
PKG_SHA256="dc75770ee448249cfeca4f520ca0ddae25d13adace39d3a20c1bb850b0502817"
PKG_LICENSE="BSD-3-Clause"
PKG_SITE="https://github.com/libretro/quasi88-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A port of QUASI88, a PC-8800 series emulator by Showzoh Fukunaga, to the libretro API"

pre_configure_target() {
  CFLAGS="${CFLAGS} -std=gnu17"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a quasi88_libretro.so ${INSTALL}/usr/lib/libretro
}
