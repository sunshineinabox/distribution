# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vitaquake3-lr"
PKG_VERSION="61374f650f42c4f25c215ea1f8db992af162ada6"
PKG_SHA256="30edb3773c4a87eb3d6d7ebdf256b1e760df228494da0782fc102539798b8409"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/vitaquake3"
PKG_URL="https://github.com/libretro/vitaquake3/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Quake III - ioquake3 port for libretro"

if [ "${OPENGL_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL}"
elif [ "${OPENGLES_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
fi

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a vitaquake3_libretro.so $INSTALL/usr/lib/libretro
}
