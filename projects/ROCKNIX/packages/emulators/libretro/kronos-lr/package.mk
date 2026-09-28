# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="kronos-lr"
PKG_VERSION="d451a55253e2e75bcef704ec8ade2085d298212c"
PKG_SHA256="941c264771c0e89e914ad75827cc28ed6eecceac2c2c0cce358f1a0a598fd59c"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/FCare/Kronos"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain boost zlib"
PKG_LONGDESC="Kronos is a Sega Saturn emulator forked from yabause."
PKG_TOOLCHAIN="make"

case ${ARCH} in
  aarch64) platform="platform=arm64" ;;
  x86_64) platform="" ;;
esac

make_target() {
  make -C ${PKG_BUILD}/yabause/src/libretro/ generate-files CC="${HOSTCC}"
  make -C ${PKG_BUILD}/yabause/src/libretro/ ${platform} HAVE_CDROM=1 FORCE_GLES=0
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a ${PKG_BUILD}/yabause/src/libretro/kronos_libretro.so ${INSTALL}/usr/lib/libretro/kronos_libretro.so
}
