# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="vice-sa"
PKG_VERSION="3.10"
PKG_SHA256="8e5bac18cbcb9f192380ad3ef881f8790f5b75c41d7b3da65d831985d864d6d1"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://sourceforge.net/projects/vice-emu"
PKG_URL="${PKG_SITE}/files/releases/vice-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain xa:host SDL2 SDL2_image ncurses readline busybox:host"
PKG_LONGDESC="Commodore 8-bit Emulator"

PKG_CONFIGURE_OPTS_TARGET+=" --disable-pdf-docs --enable-gtk3ui=no --without-alsa --with-pulse --enable-sdl2ui"

if [ ! "${OPENGL}" = "no" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu libglvnd"
fi

if [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
fi

pre_configure_target() {
  export LDFLAGS="${LDFLAGS} -lreadline -lncursesw -ltinfow"
  export CFLAGS="${CFLAGS} -fcommon"
}

post_makeinstall_target() {
  mkdir -p ${INSTALL}/usr/config/vice
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/vice

  for sc in x128 x64sc xplus4 xvic; do
    cp -a ${PKG_DIR}/scripts/start_vice.sh ${INSTALL}/usr/bin/start_${sc}.sh
    sed -i "s~@EMU@~${sc}~g" ${INSTALL}/usr/bin/start_${sc}.sh
  done
}
