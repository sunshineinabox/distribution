# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="android-headers"
PKG_VERSION="217a38e92cc64a96b4d1ac7469f30827d84485d3"
PKG_SHA256="14668ac24a4fde9f17211b8649b23e6755777f18af15c8a703e5034a4ebbadb6"
PKG_LICENSE="Apache-2.0"
PKG_SITE="https://github.com/Halium/android-headers"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Android 11 (halium-11.0) headers for libhybris"
PKG_TOOLCHAIN="manual"

makeinstall_target() {
  make PREFIX=/usr DESTDIR=${SYSROOT_PREFIX} install
}
