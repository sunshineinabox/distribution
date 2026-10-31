# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mt6785-connectivity"
PKG_VERSION="da90ba9db76a004336f0e0b71acbf76fac39743d"
PKG_SHA256="55e898bb4590a19914d99580922e75f859d6ab42c4a4714dfae4307a70ec89aa"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/sunshineinabox/mt6785-connectivity"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain linux"
PKG_LONGDESC="MediaTek CONNSYS WMT, BTIF, gen4m Wi-Fi and Bluetooth drivers for MT6785, built out of tree"
PKG_TOOLCHAIN="manual"
PKG_IS_KERNEL_PKG="yes"

pre_make_target() {
  unset LDFLAGS
}

make_target() {
  kernel_make -C $(kernel_path) M=${PKG_BUILD}
  ${CC} ${CFLAGS} -o wmt_loader wmt_loader.c
}

makeinstall_target() {
  mkdir -p ${INSTALL}/$(get_full_module_dir)/${PKG_NAME}
    find . -name '*.ko' -exec cp {} ${INSTALL}/$(get_full_module_dir)/${PKG_NAME} \;

  mkdir -p ${INSTALL}/usr/bin
    cp wmt_loader mt6785-connectivity ${INSTALL}/usr/bin
    chmod 0755 ${INSTALL}/usr/bin/mt6785-connectivity
}

post_install() {
  enable_service mt6785-connectivity.service
}
