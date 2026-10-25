# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="libmd"
PKG_VERSION="1.2.0"
PKG_SHA256="ac15ffb8430502fbaccdec66c5a82ee0eab0b0f36220df56710feadfeb13d0a0"
PKG_LICENSE="BSD-3-Clause"
PKG_SITE="https://www.hadrons.org/software/libmd/"
PKG_URL="https://libbsd.freedesktop.org/releases/${PKG_NAME}-${PKG_VERSION}.tar.xz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Message digest functions from BSD systems"
PKG_TOOLCHAIN="configure"

PKG_CONFIGURE_OPTS_TARGET="--enable-static --disable-shared --with-pic"
