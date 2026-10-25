# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="lvm2"
PKG_VERSION="2.03.35"
PKG_SHA256="ebf28b3427535e2b5abd9991cd839b61622a0dbfb8c86df0f7af1f69dcaa8371"
PKG_LICENSE="GPL-2.0-only LGPL-2.1-only"
PKG_SITE="https://sourceware.org/lvm2/"
PKG_URL="https://sourceware.org/pub/lvm2/LVM2.${PKG_VERSION}.tgz"
PKG_SOURCE_DIR="LVM2.${PKG_VERSION}"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="libdevmapper and dmsetup from LVM2"
PKG_TOOLCHAIN="configure"

PKG_CONFIGURE_OPTS_TARGET="ac_cv_func_malloc_0_nonnull=yes \
                           ac_cv_func_realloc_0_nonnull=yes \
                           --disable-nls \
                           --disable-readline \
                           --disable-selinux \
                           --disable-blkid_wiping \
                           --disable-udev_sync \
                           --disable-udev_rules \
                           --enable-pkgconfig \
                           --with-default-run-dir=/run/lvm \
                           --with-default-locking-dir=/run/lock/lvm"

PKG_MAKE_OPTS_TARGET="device-mapper"

pre_configure_target() {
  cd ${PKG_BUILD}
  rm -rf .${TARGET_NAME}
}

makeinstall_target() {
  make install_device-mapper DESTDIR=${SYSROOT_PREFIX}
  make install_device-mapper DESTDIR=${INSTALL}
}
