# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="stb"
PKG_VERSION="2c980bb59875b0d32144a71867fbdebb2f77cd20"
PKG_SHA256="9a955b1b49a4410088a2e0ee2a9c057c3c907d0c1d75454144cb980aca0ba515"
PKG_LICENSE="MIT OR Unlicense"
PKG_SITE="https://github.com/nothings/stb"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Single-file public domain libraries for C/C++"
PKG_TOOLCHAIN="manual"

makeinstall_target() {
  mkdir -p ${SYSROOT_PREFIX}/usr/include/stb
    cp -p ${PKG_BUILD}/*.h ${SYSROOT_PREFIX}/usr/include/stb
}
