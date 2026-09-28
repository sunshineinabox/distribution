# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="date"
PKG_VERSION="3.0.5"
PKG_SHA256="ef786edc203daec76475825640b3af247bd08e31fc52217e5ce8f76107b4bb05"
PKG_LICENSE="MIT"
PKG_SITE="https://github.com/HowardHinnant/date"
PKG_URL="${PKG_SITE}/archive/v${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A date and time library based on the C++11/14/17 <chrono> header"

PKG_CMAKE_OPTS_TARGET="-DBUILD_TZ_LIB=OFF \
                       -DENABLE_DATE_TESTING=OFF \
                       -DENABLE_DATE_INSTALL=ON"
