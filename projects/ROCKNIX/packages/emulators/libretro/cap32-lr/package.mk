# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="cap32-lr"
PKG_VERSION="af5a98fc0e7d316810bde032dc3eff9596c75956"
PKG_SHA256="c9a535fe3b56bd31edff359c2a95d75f87cbc3f5877b099bbedf9227d38575f4"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/libretro-cap32"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="caprice32 4.2.0 libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a cap32_libretro.so ${INSTALL}/usr/lib/libretro
}
