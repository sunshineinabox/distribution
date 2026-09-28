# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="snes9x2005_plus-lr"
PKG_VERSION="a79dfe9047e7fec58808aefe48ad2bf499c7af11"
PKG_SHA256="ed988a94a83ac67c2f8afa89dd7d444e61dc2f47ab9e4c75d8c90a46ce779975"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/snes9x2005"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Snes9x 2005 Plus."

PKG_MAKE_OPTS_TARGET="USE_BLARGG_APU=1 platform=armv8-hardfloat-neon"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a snes9x2005_plus_libretro.so ${INSTALL}/usr/lib/libretro
}
