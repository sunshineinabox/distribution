# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="smsplus-gx-lr"
PKG_VERSION="3844b46caa926b6494987b97da63092818c4ddef"
PKG_SHA256="09ca73d206f11e476e714b5297f1efdf4ca52df093103872f9c1cf76b12aa2ed"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/smsplus-gx"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="SMS Plus GX is an enhanced version"

PKG_MAKE_OPTS_TARGET="-f Makefile.libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a smsplus_libretro.so ${INSTALL}/usr/lib/libretro
}
