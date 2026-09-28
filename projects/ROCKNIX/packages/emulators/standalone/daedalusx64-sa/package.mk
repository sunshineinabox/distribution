# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="daedalusx64-sa"
PKG_VERSION="4f5c6fb045358044b64173fac619db5496cc2328"
PKG_SHA256="a9894936b7f7619a785be727cd3a6d409b694b54a64967e14dde3430b2be8f1d"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/DaedalusX64/daedalus"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain libfmt SDL2 SDL2_ttf glew mesa glm"
PKG_LONGDESC="DaedalusX64 is a Nintendo 64 emulator for PSP, 3DS, Vita, Linux, macOS and Windows"

if [ "${ARCH}" = "aarch64" ]; then
  PKG_TOOLCHAIN="manual"
else
  PKG_TOOLCHAIN="cmake"
fi

if [ "${OPENGL_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL}"
elif [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
fi

post_unpack() {
  sed -i 's|message (${CMAKE_CXX_FLAGS_RELEASE})|message("${CMAKE_CXX_FLAGS_RELEASE}")|g' ${PKG_BUILD}/Source/CMakeLists.txt
}

makeinstall_target() {
  if [ "${ARCH}" = "aarch64" ]; then
    mkdir -p ${INSTALL}
      cp -a ${ROOT}/build.${DISTRO}-${DEVICE}.arm/install_pkg/daedalusx64-sa-${PKG_VERSION}/usr ${INSTALL}
  else
    mkdir -p ${INSTALL}/usr/bin
      cp -a ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin

    mkdir -p ${INSTALL}/usr/config/DaedalusX64
      cp -a ${PKG_BUILD}/.${TARGET_NAME}/Source/daedalus ${INSTALL}/usr/config/DaedalusX64/daedalus
      cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/DaedalusX64
      cp -a ${PKG_BUILD}/Data/* ${INSTALL}/usr/config/DaedalusX64
      cp -a ${PKG_BUILD}/Source/SysGL/HLEGraphics/n64.psh ${INSTALL}/usr/config/DaedalusX64
  fi
}
