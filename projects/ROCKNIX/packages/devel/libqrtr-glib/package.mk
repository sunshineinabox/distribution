# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="libqrtr-glib"
PKG_VERSION="1.4.0"
PKG_SHA256="9e5f988c6005af347f6d0ac95c872e473c9c10c3ec3714ee0b73d896db7d5766"
PKG_LICENSE="LGPL-2.1-or-later"
PKG_SITE="https://gitlab.freedesktop.org/mobile-broadband/libqrtr-glib"
PKG_URL="${PKG_SITE}/-/archive/${PKG_VERSION}/${PKG_NAME}-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain glib"
PKG_LONGDESC="GLib library for the Qualcomm IPC Router (QRTR) bus"

PKG_MESON_OPTS_TARGET="-Dintrospection=false \
                       -Dgtk_doc=false"
