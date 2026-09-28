# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vircon32-lr"
PKG_VERSION="dd78c5cea1fcaa382b7492d0d813d6c1534914d7"
PKG_SHA256="43778b0fa9210bcbe2507a83ce95a3eef4122ff8bfa7e190a9b4741b56f7eefe"
PKG_LICENSE="BSD-3-Clause"
PKG_SITE="https://github.com/vircon32/vircon32-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain ${OPENGLES}"
PKG_LONGDESC="Vircon32 32-bit Virtual Console"
PKG_TOOLCHAIN="cmake-make"

PKG_CMAKE_OPTS_TARGET="-DPLATFORM=EMUELEC \
                       -DOPENGL_INCLUDE_DIR=${SYSROOT_PREFIX}/usr/include \
                       -DCMAKE_RULE_MESSAGES=OFF \
                       -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"

if [ "${PREFER_GLES}" = "yes" ]; then
  PKG_CMAKE_OPTS_TARGET+=" -DENABLE_OPENGLES2=1"
fi

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a vircon32_libretro.so ${INSTALL}/usr/lib/libretro
}
