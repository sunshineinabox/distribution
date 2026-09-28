# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="pcsx_rearmed-lr"
PKG_VERSION="ff81ed17a15241d2f3730cdd7585b38e172532ca"
PKG_SHA256="23e0006435e66503e1b862daaf8a47cb6bee120f4ca82482c08ec129668a35c5"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/pcsx_rearmed"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="ARM optimized PCSX fork"
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro -C .. GIT_VERSION=${PKG_VERSION} platform=$(if [ "${TARGET_ARCH}" = "x86_64" ]; then echo unix; else echo ${DEVICE}; fi)"

post_unpack() {
  sed -i 's/\-O[23]/-Ofast/' ${PKG_BUILD}/Makefile
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    case ${TARGET_ARCH} in
      aarch64|x86_64) cp -a ../pcsx_rearmed_libretro.so ${INSTALL}/usr/lib/libretro ;;
      arm) cp -a ../pcsx_rearmed_libretro.so ${INSTALL}/usr/lib/libretro/pcsx_rearmed32_libretro.so ;;
    esac
}
