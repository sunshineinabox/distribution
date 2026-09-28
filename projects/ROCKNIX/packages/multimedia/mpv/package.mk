# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019-present Shanti Gilbert (https://github.com/shantigilbert)
# Copyright (C) 2023 JELOS (https://github.com/JustEnoughLinuxOS)
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mpv"
PKG_VERSION="41f6a645068483470267271e1d09966ca3b9f413" # 0.41.0
PKG_SHA256="068960e89211f2adc80af03f637fa393fb5e407d3dd23592c8fe8c5490bb7ae4"
PKG_LICENSE="GPLv2+"
PKG_SITE="https://github.com/mpv-player/mpv"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain ffmpeg SDL2 luajit libass libplacebo libdrm"
PKG_LONGDESC="Video player based on MPlayer/mplayer2 https://mpv.io"

if [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
  # gl covers GLES too, and the Mali blob cannot create a vulkan wayland swapchain
  PKG_MESON_OPTS_TARGET+=" -Dgl=enabled -Degl=enabled"
# elif: devices with both GLES and GL would otherwise end up with egl=disabled
elif [ "${OPENGL_SUPPORT}" = "yes" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu libglvnd"
  PKG_MESON_OPTS_TARGET+=" -Dgl=enabled -Degl=disabled"
fi

if [ "${DISPLAYSERVER}" = "wl" ]; then
  PKG_MESON_OPTS_TARGET+=" -Dwayland=enabled"
else
  PKG_MESON_OPTS_TARGET+=" -Dwayland=disabled"
fi

# Vulkan has issues on S922X so disable
[ "${DEVICE}" == "S922X" ] && PKG_MESON_OPTS_TARGET+=" -Dvulkan=disabled"

# 0.41 dropped -Dsdl2, so name the features to make a missing SDL2 an error
PKG_MESON_OPTS_TARGET+=" -Dsdl2-audio=enabled -Dsdl2-video=enabled -Dsdl2-gamepad=enabled"

# vaapi only has a driver on x86_64 (mesa radeonsi)
if [ "${TARGET_ARCH}" = "x86_64" ]; then
  PKG_DEPENDS_TARGET+=" libva"
  PKG_MESON_OPTS_TARGET+=" -Dvaapi=enabled"
fi

# DRM gates the drm hwdecs that ffmpeg's v4l2-request hwaccels need
PKG_MESON_OPTS_TARGET+=" -Ddrm=enabled"

post_makeinstall_target() {
  cp ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/* 2>/dev/null ||:
  mkdir -p ${INSTALL}/usr/config/mpv
  cp -rf ${PKG_DIR}/config/* ${INSTALL}/usr/config/mpv/

  # /etc/mpv updates with the image, unlike /usr/config which is only copied once
  mkdir -p ${INSTALL}/etc/mpv
    cp ${PKG_DIR}/config/mpv.conf ${INSTALL}/etc/mpv/
}
