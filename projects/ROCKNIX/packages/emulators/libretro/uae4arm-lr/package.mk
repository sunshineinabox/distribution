# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="uae4arm-lr"
PKG_VERSION="fc1cb90afd6b5c6d9bb933d111c4c99c90f37688"
PKG_SHA256="e9d9626e1af3f046adb8495a3ac79f69154e77065d593463e0a1f3faf63ca9df"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/Chips-fr/uae4arm-rpi"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain flac mpg123"
PKG_LONGDESC="Port of uae4arm for libretro (rpi/android)"

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro platform=unix_aarch64"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a uae4arm_libretro.so ${INSTALL}/usr/lib/libretro
}
