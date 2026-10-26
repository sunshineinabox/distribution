# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="extra-firmware"
PKG_VERSION="925dfb5efbe5d6c3ca654f81e3e1f367f8492777"
PKG_SHA256="b994fa1c451708bc7addd256e68e0a872559e54e4ddec27f73e6197d64aefc0c"
PKG_LICENSE="proprietary"
PKG_SITE="https://github.com/ROCKNIX/extra-firmware"
PKG_URL="https://github.com/ROCKNIX/extra-firmware/archive/${PKG_VERSION}.tar.gz"
PKG_LONGDESC="extra-firmware: Extra kernel firmware needed for ROCKNIX devices"
PKG_TOOLCHAIN="manual"

makeinstall_target() {
  local fw=${INSTALL}/$(get_full_firmware_dir)
  mkdir -p ${fw}

  # msm-firmware-loader provides the DSP, zap and amp blobs from the Android
  # partitions
  case "${DEVICE}" in
    "SM4450")
      cp -a SM4450/* ${fw}
      rm -r ${fw}/qcom/sm4450
      ;;
    "SM6115")
      cp -a SM6115/* ${fw}
      rm -r ${fw}/qcom/sm6115/mangmi
      ;;
    "SM8250")
      cp -a SM8250/* ${fw}
      # unreferenced, the Thor Lite uses the generic ADSP
      rm ${fw}/qcom/sm8250/ayn/thor/adsp.mbn
      ;;
    "SM8550")
      cp -a SM8550/* ${fw}
      rm -r ${fw}/qcom/sm8550/ayaneo
      rm ${fw}/qcom/sm8550/ayn/*.mbn ${fw}/qcom/sm8550/ayn/*/*.mbn ${fw}/qcom/sm8550/ayn/*/*.jsn
      ;;
    "SM8650")
      cp -a SM8650/* ${fw}
      rm -r ${fw}/qcom/sm8650/ayaneo
      ;;
    "SM8750")
      cp -a SM8750/* ${fw}
      rm -r ${fw}/qcom/sm8750/konkr
      rm ${fw}/qcom/sm8750/gen80000_zap.mbn ${fw}/qcom/sm8750/ayn/*/*.mbn ${fw}/qcom/sm8750/ayn/*/*.jsn
      ;;
    "RK3566") cp -a RK3566/* ${fw} ;;
  esac
}
