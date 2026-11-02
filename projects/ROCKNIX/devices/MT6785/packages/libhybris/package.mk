# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="libhybris"
PKG_VERSION="7079712a42ea2754adf747e70c6cc75764c8596e"
PKG_SHA256="5ea95bd6596bfac95414f911289dee8fbf8a7e076d274e07fa338d335b0205dc"
PKG_LICENSE="Apache-2.0"
PKG_SITE="https://github.com/libhybris/libhybris"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain android-headers vulkan-headers"
PKG_LONGDESC="Bionic compatibility layer to load the stock Android 11 Mali blob from glibc"
PKG_TOOLCHAIN="autotools"

configure_package() {
  PKG_CONFIGURE_SCRIPT="${PKG_BUILD}/hybris/configure"

  PKG_CONFIGURE_OPTS_TARGET="--enable-arch=arm64 \
                             --enable-mali-quirks \
                             --with-android-headers=${SYSROOT_PREFIX}/usr/include/android \
                             --with-default-egl-platform=null \
                             --with-default-hybris-ld-library-path=/vendor/lib64/egl:/vendor/lib64/hw:/vendor/lib64:/apex/com.android.vndk.v30/lib64 \
                             --libdir=/usr/lib/libhybris \
                             --includedir=/usr/include/libhybris"
}

pre_configure_target() {
  CFLAGS+=" -DVK_ENABLE_BETA_EXTENSIONS -D_GNU_SOURCE"
}

post_makeinstall_target() {
  rm -rf ${INSTALL}/usr/bin ${INSTALL}/usr/include
  find ${INSTALL}/usr/lib/libhybris -name "*.la" -delete
}
