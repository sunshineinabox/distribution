# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mojozork-lr"
PKG_VERSION="ff7e00742a00acec8e175ddefb97520fb270df2d"
PKG_SHA256="44d9512bde049318e7431d3494e986725cadac2ab6e0b8c82858645c723651a8"
PKG_LICENSE="Zlib"
PKG_SITE="https://github.com/icculus/mojozork"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain sqlite SDL3"
PKG_LONGDESC="A simple Z-Machine implementation in a single C file"

PKG_CMAKE_OPTS_TARGET="-DMOJOZORK_LIBRETRO=ON \
                       -DMOJOZORK_STANDALONE_DEFAULT=OFF \
                       -DMOJOZORK_MULTIZORK_DEFAULT=OFF \
                       -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a mojozork_libretro.so ${INSTALL}/usr/lib/libretro
}
