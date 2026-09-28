# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="supersnes9x-lr"
PKG_VERSION="86fcda70d343696031bab962ab0725a28e740e0e"
PKG_SHA256="ae70fc47119461978a4fac9e28b8ae11227c905fb91bd982398b465dadf53091"
PKG_LICENSE="Non-commercial"
PKG_SITE="https://github.com/shanytc/snes9x"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="SuperSnes9x is a Snes9x-based core extended to also load and run Game Boy (.gb), Super Game Boy (.sgb), and Game Boy Color (.gbc) content via the bundled SGB subsystem, in addition to SNES/SFC, Satellaview and Sufami Turbo. Game Boy and Super Game Boy are fully supported (BIOS or BIOS-less); .gbc runs in monochrome DMG-compatibility mode. Place a real Super Game Boy BIOS (sgb.sfc / sgb2.sfc) in the system directory for authentic SGB sound and borders."
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="-C libretro"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a libretro/supersnes9x_libretro.so ${INSTALL}/usr/lib/libretro
}
