# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-present Team LibreELEC (https://libreelec.tv)

. ${ROOT}/packages/wayland/libxkbcommon/package.mk

# keep X11 support under wayland for XWayland clients, which core turns off
if [ "${DISPLAYSERVER}" = "wl" ]; then
  PKG_DEPENDS_TARGET+=" libXau libxcb"
  PKG_MESON_OPTS_TARGET="${PKG_MESON_OPTS_TARGET/-Denable-x11=false/-Denable-x11=true}"
  PKG_MESON_OPTS_TARGET+=" -Dxkb-config-root=/usr/share/X11/xkb"
fi

pre_configure_target() {
  if [ "${DISPLAYSERVER}" = "x11" -o "${DISPLAYSERVER}" = "wl" ]; then
    TARGET_LDFLAGS="${LDFLAGS} -lXau -lxcb"
  fi
}
