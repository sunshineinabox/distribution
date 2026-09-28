# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. ${ROOT}/packages/network/openssh/package.mk

post_install() {
  enable_service sshd.service
}
