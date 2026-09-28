# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. ${ROOT}/packages/wayland/lib/seatd/package.mk

PKG_BUILD_FLAGS="${PKG_BUILD_FLAGS/-sysroot/}"

post_install() {
  enable_service seatd.service
}
