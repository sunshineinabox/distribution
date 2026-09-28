# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="puae-lr"
PKG_VERSION="6536174a80d74e6c325aaa5390ff091fac8761d0"
PKG_SHA256="f0f7aeefc156178cc5b6c85f786964e67328557d2a930f9fbab658c65c403e82"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/libretro-uae"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="WIP libretro port of UAE (P-UAE and libco) Expect bugs"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a puae_libretro.so ${INSTALL}/usr/lib/libretro
}
