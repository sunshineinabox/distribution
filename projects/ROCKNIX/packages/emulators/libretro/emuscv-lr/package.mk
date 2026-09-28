# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="emuscv-lr"
PKG_VERSION="17407117018919545428b753277dabd83630052f"
PKG_SHA256="99fe167d0e278aef4a5f4c4ace7940f949ae03b2251b0d49946a699a39d2e454"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://gitlab.com/MaaaX-EmuSCV/libretro-emuscv"
PKG_URL="${PKG_SITE}/-/archive/${PKG_VERSION}/libretro-emuscv-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain bin2c:host"
PKG_DEPENDS_UNPACK="glibc"
PKG_LONGDESC="An EPOCH/YENO Super Cassette Vision (1984) home video game emulator for Libretro"
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="platform=unix"

pre_configure_target() {
  export TERM=xterm
  CXXFLAGS+=" -I$(get_build_dir glibc)/sysdeps/unix/sysv/linux/x86"
  sed -i 's~tools/bin2c/~'${TOOLCHAIN}'/usr/bin/~g' Makefile.libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ${PKG_BUILD}/emuscv_libretro.so ${INSTALL}/usr/lib/libretro
}
