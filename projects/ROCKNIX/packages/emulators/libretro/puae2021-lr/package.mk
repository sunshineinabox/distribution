# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="puae2021-lr"
PKG_VERSION="297b34777372fd5f117785798f99b4c7433b6034"
PKG_SHA256="53a6e7f49e479af4c5f6e35dfef8ad7fc5ea95231440024bcc4f085375683d08"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/libretro-uae"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="PUAE 2021 libretro port of UAE"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a puae_libretro.so ${INSTALL}/usr/lib/libretro/puae2021_libretro.so
}
