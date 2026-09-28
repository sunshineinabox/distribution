# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mame-sa"
PKG_VERSION="0.289"
PKG_SHA256="3aeef8a585efdac080d1dc276ba906aff1c82faece25b439a4e1a112d269375f"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/mamedev/mame"
PKG_URL="${PKG_SITE}/archive/mame${PKG_VERSION/./}.tar.gz"
PKG_DEPENDS_TARGET="toolchain SDL2 SDL2_ttf fontconfig freetype zlib flac sqlite expat ${OPENGL} glu libglvnd"
PKG_LONGDESC="MAME - Multiple Arcade Machine Emulator, SDL frontend"
PKG_TOOLCHAIN="make"
PKG_BUILD_FLAGS="-lto"

case ${TARGET_ARCH} in
  aarch64)
    MAME_PLATFORM="PLATFORM=arm64"
    ;;
esac

PKG_MAKE_OPTS_TARGET="REGENIE=1 \
                      VERBOSE=1 \
                      NOWERROR=1 \
                      IGNORE_GIT=1 \
                      OPENMP=1 \
                      TARGETOS=linux \
                      CROSS_BUILD=1 \
                      PTR64=1 \
                      ${MAME_PLATFORM} \
                      OSD=sdl \
                      TARGET=mame \
                      SUBTARGET=mame \
                      TOOLS=0 \
                      USE_QTDEBUG=0 \
                      NO_X11=1 \
                      NO_USE_XINPUT=1 \
                      NO_USE_MIDI=1 \
                      NO_USE_PORTAUDIO=1 \
                      NO_USE_PULSEAUDIO=1 \
                      NO_USE_PIPEWIRE=1 \
                      PYTHON_EXECUTABLE=python3 \
                      USE_SYSTEM_LIB_EXPAT=1 \
                      USE_SYSTEM_LIB_ZLIB=1 \
                      USE_SYSTEM_LIB_FLAC=1 \
                      USE_SYSTEM_LIB_SQLITE3=1"

make_target() {
  unset ARCH
  unset DISTRO
  unset PROJECT
  # The explicit "linux" goal builds the native configuration, "all" would add -m64
  make ${PKG_MAKE_OPTS_TARGET} OVERRIDE_CC=${CC} OVERRIDE_CXX=${CXX} OVERRIDE_LD=${LD} AR=${AR} ${MAKEFLAGS} linux
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -a ${PKG_BUILD}/mame ${INSTALL}/usr/bin/mame
    cp -a ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/usr/share/mame
    cp -a ${PKG_BUILD}/{artwork,bgfx,ctrlr,plugins,uismall.bdf} ${INSTALL}/usr/share/mame
    rm -rf ${INSTALL}/usr/share/mame/bgfx/shaders/{dx9,dx11,metal}

  mkdir -p ${INSTALL}/usr/config/mame
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/mame
}
