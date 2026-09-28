#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

. /etc/profile
set_kill set "ymir"

# Load gptokeyb support files
control-gen_init.sh
source /storage/.config/gptokeyb/control.ini
get_controls

IMMUTABLE_CONF_DIR="/usr/config/ymir"
CONF_DIR="/storage/.config/ymir"
CONF_FILE="${CONF_DIR}/Ymir.toml"
SAVESTATES_DIR="/storage/roms/savestates/saturn/ymir"

[ ! -d "${CONF_DIR}" ] && cp -r "${IMMUTABLE_CONF_DIR}" /storage/.config
[ ! -f "${CONF_FILE}" ] && cp "${IMMUTABLE_CONF_DIR}/Ymir.toml" "${CONF_FILE}"
[ ! -f "${CONF_DIR}/ymir.gptk" ] && cp "${IMMUTABLE_CONF_DIR}/ymir.gptk" "${CONF_DIR}"
[ ! -d "${SAVESTATES_DIR}" ] && mkdir -p "${SAVESTATES_DIR}"

# Ymir rewrites Ymir.toml on exit, so every key set here stays present
set_toml() {
  sed -i "s|^\([[:space:]]*${1}[[:space:]]*=\).*|\1 ${2}|" "${CONF_FILE}"
}

# Emulation Station Features
GAME=$(echo "${1}"| sed "s#^/.*/##")
PLATFORM=$(echo "${2}"| sed "s#^/.*/##")
FPS=$(get_setting show_fps "${PLATFORM}" "${GAME}")
INTEGER_SCALING=$(get_setting integer_scaling "${PLATFORM}" "${GAME}")
DEINTERLACE=$(get_setting deinterlace "${PLATFORM}" "${GAME}")
SH2_CACHE=$(get_setting emulate_sh2_cache "${PLATFORM}" "${GAME}")

# Set the cores to use
CORES=$(get_setting "cores" "${PLATFORM}" "${GAME}")
unset EMUPERF
[ "${CORES}" = "little" ] && EMUPERF="${SLOW_CORES}"
[ "${CORES}" = "big" ] && EMUPERF="${FAST_CORES}"

# Show FPS - default to off
[ "${FPS}" = "true" ] && set_toml ShowFrameRateOSD true || set_toml ShowFrameRateOSD false

# Integer scaling - default to off
[ "${INTEGER_SCALING}" = "true" ] && set_toml ForceIntegerScaling true || set_toml ForceIntegerScaling false

# Progressive rendering of interlaced modes - default to off
[ "${DEINTERLACE}" = "true" ] && set_toml Deinterlace true || set_toml Deinterlace false

# SH-2 cache emulation, needed by a few games - default to off
[ "${SH2_CACHE}" = "true" ] && set_toml EmulateSH2Cache true || set_toml EmulateSH2Cache false

set_toml FullScreen true

echo "GAME set to: ${GAME}"
echo "PLATFORM set to: ${PLATFORM}"
echo "CPU CORES set to: ${EMUPERF}"
echo "FPS set to: ${FPS}"
echo "INTEGER_SCALING set to: ${INTEGER_SCALING}"
echo "DEINTERLACE set to: ${DEINTERLACE}"
echo "SH2_CACHE set to: ${SH2_CACHE}"
echo "Launching /usr/bin/ymir -f -p ${CONF_DIR} ${1}"

${GPTOKEYB} ymir -c "${CONF_DIR}/ymir.gptk" &
${EMUPERF} /usr/bin/ymir -f -p "${CONF_DIR}" "${1}"
kill -9 $(pidof gptokeyb)
