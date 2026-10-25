# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="msm-firmware-loader"
PKG_VERSION="1.9.0"
PKG_SHA256="af66adb4f1bb6c1d4d7a7a5e96ccd9e8561dc897803aed58166492f0fd857e56"
PKG_LICENSE="MIT"
PKG_SITE="https://gitlab.postmarketos.org/postmarketOS/msm-firmware-loader"
PKG_URL="${PKG_SITE}/-/archive/${PKG_VERSION}/${PKG_NAME}-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain make-dynpart-mappings"
PKG_LONGDESC="Loads Qualcomm DSP firmware and HexagonFS files from the Android partitions"
PKG_TOOLCHAIN="manual"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/sbin
    install -m 0755 ${PKG_BUILD}/msm-firmware-loader.sh ${INSTALL}/usr/sbin
  mkdir -p ${INSTALL}/usr/lib/systemd/system
    cp -v ${PKG_BUILD}/msm-firmware-loader.service ${INSTALL}/usr/lib/systemd/system
}

post_install() {
  enable_service msm-firmware-loader.service
}
