# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. ${ROOT}/packages/graphics/libdrm/package.mk

post_makeinstall_target() {
  PKG_LIBDRM_LIST="drmdevice modeprint proptest vbltest"
  for PKG_LIBDRM_TEST in ${PKG_LIBDRM_LIST}; do
    safe_remove ${INSTALL}/usr/bin/${PKG_LIBDRM_TEST}
  done

  if listcontains "${GRAPHIC_DRIVERS}" "radeonsi"; then
    safe_remove ${INSTALL}/usr/bin/amdgpu_stress
  fi

  for header in xf86drm.h xf86drmMode.h; do
    sed -i "s#<drm.h>#<drm/drm.h>#g" ${SYSROOT_PREFIX}/usr/include/${header}
    sed -i "s#<drm_mode.h#<drm/drm_mode.h>#g" ${SYSROOT_PREFIX}/usr/include/${header}
  done
}
