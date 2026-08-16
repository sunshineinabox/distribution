# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019-present Shanti Gilbert (https://github.com/shantigilbert)
# Copyright (C) 2023 JELOS (https://github.com/JustEnoughLinuxOS)
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mpv"
PKG_VERSION="41f6a645068483470267271e1d09966ca3b9f413" # 0.41.0
PKG_LICENSE="GPLv2+"
PKG_SITE="https://github.com/mpv-player/mpv"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain ffmpeg SDL2 luajit libass libplacebo libdrm"
PKG_LONGDESC="Video player based on MPlayer/mplayer2 https://mpv.io"

if [ "${OPENGLES_SUPPORT}" = yes ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
  # gl is mpv's whole OpenGL/GLES renderer family, not desktop-GL: disabling
  # it left vulkan as the only vo backend, and the Mali blob cannot create a
  # wayland swapchain (VK_ERROR_INITIALIZATION_FAILED on RG-DS) - so mpv
  # decoded perfectly and displayed nothing. GLES rendering comes from
  # gl=enabled + egl, the same stack the compositor itself runs on.
  PKG_MESON_OPTS_TARGET+=" -Dgl=enabled -Degl=enabled"
# NOT an if: devices like RK3566 set OPENGLES_SUPPORT and OPENGL_SUPPORT
# together, and two matching blocks used to merge into gl=enabled
# egl=disabled (meson takes the last value) - which disables the EGL/GLES
# contexts and leaves mpv vulkan-only on exactly the devices that need GLES
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

# 0.41 dropped the -Dsdl2 umbrella and now autodetects SDL2 with
# required:false, gating the three features on it. Name them so a missing
# SDL2 is an error rather than three silently absent outputs.
PKG_MESON_OPTS_TARGET+=" -Dsdl2-audio=enabled -Dsdl2-video=enabled -Dsdl2-gamepad=enabled"

# vaapi is only meaningful where a va driver exists (AMD64: mesa radeonsi);
# make it deterministic there instead of relying on sysroot autodetection.
# On ARM there is no va backend - v4l2-request is the equivalent.
if [ "${TARGET_ARCH}" = "x86_64" ]; then
  PKG_DEPENDS_TARGET+=" libva"
  PKG_MESON_OPTS_TARGET+=" -Dvaapi=enabled"
fi

# DRM support gates the drm/drm-copy hwdecs that reach ffmpeg's
# v4l2-request hwaccels - without it the hantro/rkvdec decoders have no
# userspace consumer at all ("Unsupported hwdec: drm-copy" on RK3566)
PKG_MESON_OPTS_TARGET+=" -Ddrm=enabled"

post_makeinstall_target() {
  cp ${PKG_DIR}/scripts/* ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/* 2>/dev/null ||:
  mkdir -p ${INSTALL}/usr/config/mpv
  cp -rf ${PKG_DIR}/config/* ${INSTALL}/usr/config/mpv/

  # system defaults: mpv reads /etc/mpv/mpv.conf before user config, and
  # unlike /usr/config -> /storage this updates with the image - existing
  # installs never re-rsync /usr/config, which left mpv.conf undeployed
  mkdir -p ${INSTALL}/etc/mpv
    cp ${PKG_DIR}/config/mpv.conf ${INSTALL}/etc/mpv/
}
