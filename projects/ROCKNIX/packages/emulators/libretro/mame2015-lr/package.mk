# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mame2015-lr"
PKG_VERSION="7adbf440d5f554097f58f8c6ebb673adc4320a31"
PKG_SHA256="4988393d26e17d34877f6a962021ee9ec77a2276a34f6b2c2868ebb90933f1d9"
PKG_LICENSE="MAME"
PKG_SITE="https://github.com/libretro/mame2015-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Late 2014/Early 2015 version of MAME (0.160-ish) for libretro. Compatible with MAME 0.160 romsets."
PKG_BUILD_FLAGS="-lto"

pre_make_target() {
  export REALCC=${CC}
  export CC=${CXX}
  export LD=${CXX}
  make maketree
}

pre_configure_target() {
  case ${ARCH} in
    arm|aarch64)
      PKG_MAKE_OPTS_TARGET=" platform=armv8-neon-hardfloat-cortex-a53"
      sed -i 's/LDFLAGS += -Wl,--fix-cortex-a8 -Wl,--no-as-needed//g' Makefile
      sed -i 's/CCOMFLAGS += -mstructure-size-boundary=32//g' Makefile
      ;;
  esac
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mame2015_libretro.so ${INSTALL}/usr/lib/libretro
}
