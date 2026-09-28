# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. ${ROOT}/packages/lang/nasm/package.mk

# core limits nasm to x86_64, but libvpx:host needs it on every target arch
PKG_ARCH="any"
