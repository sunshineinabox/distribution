# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="make-dynpart-mappings"
PKG_VERSION="10.2.4"
PKG_SHA256="efda3dfc781cc3582e218a7b7d9618be599eb3b484c167e17873255ddf9ae527"
PKG_LICENSE="GPL-3.0-only"
PKG_SITE="https://gitlab.com/flamingradian/make-dynpart-mappings"
PKG_URL="${PKG_SITE}/-/archive/${PKG_VERSION}/${PKG_NAME}-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain libmd lvm2 util-linux"
PKG_LONGDESC="Maps Android dynamic partitions with device-mapper"
PKG_TOOLCHAIN="make"

make_target() {
  make CC="${CC}" CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" PKG_CONFIG="${PKG_CONFIG}"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -v ${PKG_BUILD}/make-dynpart-mappings ${INSTALL}/usr/bin
}
