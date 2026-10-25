# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="libssc"
PKG_VERSION="0.4.4"
PKG_SHA256="716d6bd6b34d2d753060c6b54c9a87e34fae75b724c763bf9ef487efa3621587"
PKG_LICENSE="GPL-3.0-or-later"
PKG_SITE="https://codeberg.org/DylanVanAssche/libssc"
PKG_URL="${PKG_SITE}/archive/v${PKG_VERSION}.tar.gz"
PKG_SOURCE_DIR="libssc"
PKG_DEPENDS_TARGET="toolchain glib libqmi protobuf-c protobuf-c:host Python3"
PKG_LONGDESC="Library and ssccli tool for Qualcomm Sensor Core sensors"

post_makeinstall_target() {
  rm -rf ${INSTALL}/usr/lib/python*
  rm -rf ${INSTALL}/usr/libexec/installed-tests
}
