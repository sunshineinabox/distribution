# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="ymir-sa"
PKG_VERSION="54fead6a0001d3e4b6741a8d095ee8342266c3dc" # v0.3.3
PKG_LICENSE="GPL-3.0"
PKG_SITE="https://github.com/StrikerX3/Ymir"
PKG_URL="${PKG_SITE}.git"
PKG_GIT_SUBMODULE_DEPTH="1"
PKG_DEPENDS_TARGET="toolchain SDL3 alsa-lib curl zlib zstd nlohmann-json cereal cxxopts date neargye-semver rtmidi stb tomlplusplus"
PKG_LONGDESC="Ymir is a Sega Saturn emulator"
PKG_TOOLCHAIN="cmake"

PKG_CMAKE_OPTS_TARGET="-DCMAKE_BUILD_TYPE=Release \
                       -DYmir_DEV_BUILD=OFF \
                       -DYmir_ENABLE_TESTS=OFF \
                       -DYmir_ENABLE_SANDBOX=OFF \
                       -DYmir_ENABLE_YMDASM=OFF \
                       -DYmir_ENABLE_YMIR_HEADLESS=OFF \
                       -DYmir_ENABLE_YMIR_DBG=OFF \
                       -DYmir_ENABLE_DEVLOG=OFF \
                       -DYmir_ENABLE_IMGUI_DEMO=OFF \
                       -DYmir_ENABLE_UPDATE_CHECKS=OFF \
                       -DStb_INCLUDE_DIR=${SYSROOT_PREFIX}/usr/include/stb"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
    cp -L ${PKG_BUILD}/.${TARGET_NAME}/apps/ymir-sdl3/ymir-sdl3 ${INSTALL}/usr/bin/ymir
    cp -a ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/usr/config/ymir
    cp -a ${PKG_DIR}/config/* ${INSTALL}/usr/config/ymir
}
