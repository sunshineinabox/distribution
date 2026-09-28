# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="stella-lr"
PKG_VERSION="36db8267e443a1ddfe4fabc0a3d42ec2b2332cb4"
PKG_SHA256="6e3b797567ef12ec303a7061b3085eb8cff8bce7e067eda74b67378c5100977c"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/stella-emu/stella"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of Stella to libretro."
PKG_TOOLCHAIN="make"

case ${TARGET_ARCH} in
  aarch64) PKG_MAKE_OPTS_TARGET=" -C ../src/os/libretro platform=aarch64" ;;
  *) PKG_MAKE_OPTS_TARGET=" -C ../src/os/libretro" ;;
esac

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ../src/os/libretro/stella_libretro.so ${INSTALL}/usr/lib/libretro
}

