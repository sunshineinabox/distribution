# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="prboom-lr"
PKG_VERSION="e5db549b6c85fc500b3a559efe2dc00870521c5d"
PKG_SHA256="cfedb68cc9a01b4f6a1855ae104478f7a50d3d88a108e427c619eaf5eefa25bf"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/libretro-prboom"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="libretro implementation of Doom"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a prboom_libretro.so ${INSTALL}/usr/lib/libretro/prboom_libretro.so
}
