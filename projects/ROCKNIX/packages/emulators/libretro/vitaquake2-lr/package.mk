# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vitaquake2-lr"
PKG_VERSION="4b333fe18e8816974f04432af01f87654ae16e21"
PKG_SHA256="c91570472a0f9265d9166496bd80b7950e52555d8ea521ae0a62feade4468429"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/libretro/vitaquake2"
PKG_URL="$PKG_SITE/archive/$PKG_VERSION.tar.gz"
PKG_DEPENDS_TARGET="toolchain vitaquake2-rogue-lr vitaquake2-xatrix-lr vitaquake2-zaero-lr"
PKG_LONGDESC="Libretro port of VitaQuakeII (Quake 2 engine)"

if [ "${OPENGL_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL}"
elif [ "${OPENGLES_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
fi

pre_make_target() {
  export BUILD_SYSROOT=${SYSROOT_PREFIX}

  case ${TARGET_ARCH} in
    aarch64) PKG_MAKE_OPTS_TARGET+=" platform=${DEVICE}_rocknix" ;;
  esac
}

makeinstall_target() {
  mkdir -p $INSTALL/usr/lib/libretro
    cp -a vitaquake2_libretro.so $INSTALL/usr/lib/libretro
}
