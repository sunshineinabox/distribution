# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 JELOS (https://github.com/JustEnoughLinuxOS)

PKG_NAME="mali-bifrost"
PKG_LICENSE="GPL"
PKG_SITE="https://developer.arm.com/downloads/-/mali-drivers/bifrost-kernel"
PKG_LONGDESC="mali-bifrost: Linux drivers for Mali Bifrost GPUs"
PKG_TOOLCHAIN="manual"
PKG_IS_KERNEL_PKG="yes"

PKG_VERSION="39da994bb6fc8819e5e8c1873907dd21d17e53c1"
PKG_SHA256="f947b99a1bcb5b86ea270c1ce4cd189f4df26b3c0c8ba92c1ef256bb757afa66"
PKG_URL="http://github.com/rocknix/mali_kbase/archive/${PKG_VERSION}.tar.gz"

case ${DEVICE} in
  MT6785)
    PKG_VERSION="9253151400fda8820ac47da6ee4c31b5a8885e4f"
    PKG_SHA256="a43c0c92cf65abcac47e3730869cdff2e9dfd3f67134098c784d0bede0047f52"
    PKG_URL="https://github.com/sydarn/mali_kbase/archive/${PKG_VERSION}.tar.gz"
    ;;
esac

make_target() {
  # S922X is an actual Amlogic Meson SoC — it requires the meson platform
  # backend for correct GPU clock and power domain handling.
  # Rockchip devices use the devicetree backend which properly coordinates
  # with kernel power domain drivers during runtime PM suspend/resume cycles.
  case ${DEVICE} in
    S922X)
      MALI_PLATFORM="meson"
      EXTRA_PLATFORM_CONFIG="  CONFIG_MALI_DEVFREQ=y"
      ;;
    *)
      MALI_PLATFORM="devicetree"
      ;;
  esac

  kernel_make KDIR=$(kernel_path) -C ${PKG_BUILD}/product/kernel/drivers/gpu/arm/midgard \
       CONFIG_MALI_MIDGARD=m CONFIG_MALI_PLATFORM_NAME=${MALI_PLATFORM} CONFIG_MALI_REAL_HW=y \
       ${EXTRA_PLATFORM_CONFIG}
}

makeinstall_target() {
  DRIVER_DIR=${PKG_BUILD}/product/kernel/drivers/gpu/arm/midgard

  mkdir -p ${INSTALL}/$(get_full_module_dir)/${PKG_NAME}
    cp ${DRIVER_DIR}/mali_kbase.ko ${INSTALL}/$(get_full_module_dir)/${PKG_NAME}
}
