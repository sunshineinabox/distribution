#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

. /etc/profile
set_kill set "mame"

# Load gptokeyb support files
control-gen_init.sh
source /storage/.config/gptokeyb/control.ini
get_controls

IMMUTABLE_CONF_DIR="/usr/config/mame"
CONF_DIR="/storage/.config/mame"
SAVESTATES_DIR="/storage/roms/savestates/mame"

[ ! -d "${CONF_DIR}" ] && cp -r "${IMMUTABLE_CONF_DIR}" /storage/.config
for FILE in mame.ini ui.ini mame.gptk; do
  [ ! -f "${CONF_DIR}/${FILE}" ] && cp "${IMMUTABLE_CONF_DIR}/${FILE}" "${CONF_DIR}"
done
[ ! -d "${SAVESTATES_DIR}" ] && mkdir -p "${SAVESTATES_DIR}"

# Emulation Station Features
GAME=$(echo "${1}"| sed "s#^/.*/##")
PLATFORM=$(echo "${2}"| sed "s#^/.*/##")
GRENDERER=$(get_setting graphics_backend "${PLATFORM}" "${GAME}")
FILTER=$(get_setting bilinear_filtering "${PLATFORM}" "${GAME}")
ASPECT=$(get_setting keep_aspect_ratio "${PLATFORM}" "${GAME}")

# Set the cores to use
CORES=$(get_setting "cores" "${PLATFORM}" "${GAME}")
unset EMUPERF
[ "${CORES}" = "little" ] && EMUPERF="${SLOW_CORES}"
[ "${CORES}" = "big" ] && EMUPERF="${FAST_CORES}"

# MAME looks up sets by name, so search the game's own folder first
ROMPATH="$(dirname "${1}");/storage/roms/bios;/storage/roms/mame;/storage/roms/arcade"

case "${GRENDERER}" in
  bgfx|accel) ARGS="-video ${GRENDERER}" ;;
  *) ARGS="-video opengl" ;;
esac

[ "${FILTER}" = "false" ] && ARGS+=" -nofilter" || ARGS+=" -filter"
[ "${ASPECT}" = "false" ] && ARGS+=" -nokeepaspect" || ARGS+=" -keepaspect"

echo "GAME set to: ${GAME}"
echo "PLATFORM set to: ${PLATFORM}"
echo "CPU CORES set to: ${EMUPERF}"
echo "GRENDERER set to: ${GRENDERER}"
echo "FILTER set to: ${FILTER}"
echo "ASPECT set to: ${ASPECT}"
echo "Launching /usr/bin/mame -rompath ${ROMPATH} ${ARGS} ${GAME%.*}"

# mame.ini and ui.ini are read from the working directory
cd "${CONF_DIR}"
${GPTOKEYB} mame -c "${CONF_DIR}/mame.gptk" &
${EMUPERF} /usr/bin/mame -rompath "${ROMPATH}" ${ARGS} "${GAME%.*}"
kill -9 $(pidof gptokeyb)
