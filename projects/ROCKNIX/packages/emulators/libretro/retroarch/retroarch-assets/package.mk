# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="retroarch-assets"
PKG_VERSION="73106363e14e34c08a5854b4cfbc29f184e3b783"
PKG_SHA256="9a1ea65345b62aac6d20d19e21b3132f66770c65ad976f8badb0178219a135d7"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/libretro/retroarch-assets"
PKG_URL="https://github.com/libretro/retroarch-assets/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="make:host"
PKG_LONGDESC="RetroArch assets. Background and icon themes for the menu drivers."
PKG_TOOLCHAIN="manual"

pre_configure_target() {
  cd ../
  rm -rf .${TARGET_NAME}
}

makeinstall_target() {
  make install INSTALLDIR="${INSTALL}/usr/share/retroarch-assets"
}

post_makeinstall_target() {
  # Remove unnecesary Retroarch Assets and overlays
  rm -rf ${INSTALL}/usr/share/retroarch-assets/{branding,nxrgui,pkg,scripts,switch,wallpapers}
  rm -rf ${INSTALL}/usr/share/retroarch-assets/xmb/{automatic,dot-art,flatui,pixel,retrosystem,systematic}
}
