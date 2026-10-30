# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="pocket-air-mini-firmware"
PKG_VERSION="20261030"
PKG_LICENSE="nonfree"
PKG_SITE=""
PKG_URL=""
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Stock MT6785 connsys (Wi-Fi/BT) firmware from the AYANEO Pocket Air Mini GammaOS vendor partition, for bring-up"
PKG_TOOLCHAIN="manual"

makeinstall_target() {
  mkdir -p ${INSTALL}/$(get_full_firmware_dir)
    cp -a ${PKG_DIR}/firmware/* ${INSTALL}/$(get_full_firmware_dir)
}
