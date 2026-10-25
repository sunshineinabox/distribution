# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="libqmi"
PKG_VERSION="1.38.0"
PKG_SHA256="8ee3b3a9002f2b45c825deade85710db006e01674f5f9359d351c192cc926015"
PKG_LICENSE="LGPL-2.1-or-later"
PKG_SITE="https://gitlab.freedesktop.org/mobile-broadband/libqmi"
PKG_URL="${PKG_SITE}/-/archive/${PKG_VERSION}/${PKG_NAME}-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain glib libqrtr-glib Python3:host"
PKG_LONGDESC="GLib library for devices speaking the Qualcomm MSM Interface (QMI)"

PKG_MESON_OPTS_TARGET="-Dqrtr=true \
                       -Dudev=false \
                       -Dfirmware_update=false \
                       -Dmbim_qmux=false \
                       -Dmm_runtime_check=false \
                       -Drmnet=false \
                       -Dintrospection=false \
                       -Dman=false \
                       -Dbash_completion=false"
