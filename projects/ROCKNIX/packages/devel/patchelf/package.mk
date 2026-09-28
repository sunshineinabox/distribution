# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. ${ROOT}/packages/devel/patchelf/package.mk

# core declares no target deps, so nothing orders the target build after the toolchain
PKG_DEPENDS_TARGET="toolchain"
