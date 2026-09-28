# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vice-lr"
PKG_VERSION="9d7983826ea792f6cce7fdfe6c09488129c6f886"
PKG_SHA256="c3aaffaef10feab9d22bf8150cbbb4c2992efe6565e9b273f5269b6818e017a6"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/vice-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Versatile Commodore 8-bit Emulator version 3.0"

make_target() {
  if [ ! -d "built" ]; then
    mkdir built
  fi

  for EMUTYPE in x128 x64sc x64dtv xscpu64 xplus4 xvic xcbm5x0 xcbm2 xpet x64; do
    make clean
    make EMUTYPE=${EMUTYPE}
    mv vice_*_libretro.so built
  done
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
    cp -a built/vice_x128_libretro.so ${INSTALL}/usr/lib/libretro
    cp -a built/vice_x64_libretro.so ${INSTALL}/usr/lib/libretro
    cp -a built/vice_xplus4_libretro.so ${INSTALL}/usr/lib/libretro
    cp -a built/vice_xvic_libretro.so ${INSTALL}/usr/lib/libretro
    cp -a built/vice_xpet_libretro.so ${INSTALL}/usr/lib/libretro
}
