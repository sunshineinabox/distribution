# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mame2010-lr"
PKG_VERSION="dff8aadd1c3f38215af3955746d6e19abe0ddcea"
PKG_SHA256="ef62ec2884a18b0af64ad35a96d7622253793d5c93db315d9c1a2f2c021ae72a"
PKG_LICENSE="MAME"
PKG_SITE="https://github.com/libretro/mame2010-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Late 2010 version of MAME (0.139) for libretro. Compatible with MAME 0.139 romsets."

make_target() {
  if [ "${ARCH}" == "arm" ]; then
    make PLATCFLAGS="${CFLAGS}" PTR64=0 ARM_ENABLED=1 LCPU=arm
  elif [ "${ARCH}" == "i386" ]; then
    make PLATCFLAGS="${CFLAGS}" PTR64=0 ARM_ENABLED=0 LCPU=x86
  elif [ "${ARCH}" == "x86_64" ]; then
    make PLATCFLAGS="${CFLAGS}" PTR64=1 ARM_ENABLED=0 LCPU=x86_64
  elif [ "${ARCH}" == "aarch64" ]; then
    make PLATCFLAGS="${CFLAGS}" PTR64=1 ARM_ENABLED=1 LCPU=arm64
  fi
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mame2010_libretro.so ${INSTALL}/usr/lib/libretro
}
