#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

. /etc/profile

CORE=$(echo "${3}"| sed "s#^/.*/##")
if [ "${CORE}" = "primehack-qt" ]; then
  PRIMEHACK_BIN="primehack"
else
  PRIMEHACK_BIN="primehack-nogui"
fi

set_kill set "-9 ${PRIMEHACK_BIN}"

# control-gen supplies param_device and the SDL controller database
control-gen_init.sh
source /storage/.config/gptokeyb/control.ini
get_controls

IMMUTABLE_CONF_DIR="/usr/config/primehack"
CONF_DIR="/storage/.config/primehack"
CFG_DIR="${CONF_DIR}/Config"
DOLPHIN_INI="${CFG_DIR}/Dolphin.ini"
GFX_INI="${CFG_DIR}/GFX.ini"
GCPAD_INI="${CFG_DIR}/GCPadNew.ini"
WIIMOTE_INI="${CFG_DIR}/WiimoteNew.ini"
SAVESTATES_DIR="/storage/roms/savestates/wii/primehack"

[ ! -d "${CFG_DIR}" ] && mkdir -p "${CFG_DIR}"
[ ! -f "${WIIMOTE_INI}" ] && cp "${IMMUTABLE_CONF_DIR}/profiles/WiimoteNew.ini" "${WIIMOTE_INI}"

[ ! -d "${SAVESTATES_DIR}" ] && mkdir -p "${SAVESTATES_DIR}"
rm -rf "${CONF_DIR}/StateSaves"
ln -sf "${SAVESTATES_DIR}" "${CONF_DIR}/StateSaves"

# Emulation Station Features
GAME=$(echo "${1}"| sed "s#^/.*/##")
PLATFORM=$(echo "${2}"| sed "s#^/.*/##")
ASPECT=$(get_setting aspect_ratio "${PLATFORM}" "${GAME}")
GRENDERER=$(get_setting graphics_backend "${PLATFORM}" "${GAME}")
IRES=$(get_setting internal_resolution "${PLATFORM}" "${GAME}")
FPS=$(get_setting show_fps "${PLATFORM}" "${GAME}")
CON=$(get_setting wii_controller_profile "${PLATFORM}" "${GAME}")
HKEY=$(get_setting hotkey_enable_button "${PLATFORM}" "${GAME}")
SHADERM=$(get_setting shader_mode "${PLATFORM}" "${GAME}")
VSYNC=$(get_setting vsync "${PLATFORM}" "${GAME}")
RUMBLE=$(get_setting rumble "${PLATFORM}" "${GAME}")
WPC=$(get_setting write_protect_configs "${PLATFORM}" "${GAME}")

# Grab clean config files during boot, unless disabled in emulationstation
if [ "${WPC}" != "false" ] || [ ! -f "${DOLPHIN_INI}" ] || [ ! -f "${GFX_INI}" ] || [ ! -f "${CFG_DIR}/Hotkeys.ini" ]; then
  cp "${IMMUTABLE_CONF_DIR}/Config/Dolphin.ini" "${DOLPHIN_INI}"
  cp "${IMMUTABLE_CONF_DIR}/Config/GFX.ini" "${GFX_INI}"
  cp "${IMMUTABLE_CONF_DIR}/Config/Hotkeys.ini" "${CFG_DIR}/Hotkeys.ini"
fi

# Set the cores to use
CORES=$(get_setting "cores" "${PLATFORM}" "${GAME}")
unset EMUPERF
[ "${CORES}" = "little" ] && EMUPERF="${SLOW_CORES}"
[ "${CORES}" = "big" ] && EMUPERF="${FAST_CORES}"

# Aspect Ratio
case "${ASPECT}" in
  1|2|3) sed -i "/^AspectRatio =/c\AspectRatio = ${ASPECT}" "${GFX_INI}" ;;
  *) sed -i '/^AspectRatio =/c\AspectRatio = 0' "${GFX_INI}" ;;
esac

# Graphics Backend
case "${GRENDERER}" in
  vulkan) sed -i '/^GFXBackend =/c\GFXBackend = Vulkan' "${DOLPHIN_INI}" ;;
  opengl) sed -i '/^GFXBackend =/c\GFXBackend = OGL' "${DOLPHIN_INI}" ;;
  *) sed -i '/^GFXBackend =/c\GFXBackend = @GRENDERER@' "${DOLPHIN_INI}" ;;
esac

# Internal Resolution
case "${IRES}" in
  1|3|4|6) sed -i "/^InternalResolution =/c\InternalResolution = ${IRES}" "${GFX_INI}" ;;
  *) sed -i '/^InternalResolution =/c\InternalResolution = 2' "${GFX_INI}" ;;
esac

# Shader Mode
case "${SHADERM}" in
  0|1|2|3) sed -i "/^ShaderCompilationMode =/c\ShaderCompilationMode = ${SHADERM}" "${GFX_INI}" ;;
esac

# Show FPS
if [ "${FPS}" = "true" ]; then
  sed -i '/^ShowFPS =/c\ShowFPS = True' "${GFX_INI}"
else
  sed -i '/^ShowFPS =/c\ShowFPS = False' "${GFX_INI}"
fi

# Vsync
if [ "${VSYNC}" = "1" ]; then
  sed -i '/^VSync =/c\VSync = True' "${GFX_INI}"
else
  sed -i '/^VSync =/c\VSync = False' "${GFX_INI}"
fi

# Wii Controller Profile, custom keeps whatever was bound in the Qt UI
if [ "${CON}" != "custom" ]; then
  cp "${IMMUTABLE_CONF_DIR}/profiles/WiimoteNew.ini" "${WIIMOTE_INI}"
  [ "${RUMBLE}" = "false" ] && sed -i '/^Rumble/d' "${WIIMOTE_INI}"
fi

# nogui reads its hotkeys from the first GameCube pad, Qt uses Hotkeys.ini
if [ "${PRIMEHACK_BIN}" = "primehack-nogui" ]; then
  cp "${IMMUTABLE_CONF_DIR}/profiles/GCPadNew.ini" "${GCPAD_INI}"
else
  rm -f "${GCPAD_INI}"
fi

if [ "${HKEY}" = "mode" ]; then
  HOTKEY_BUTTON="Guide"
else
  HOTKEY_BUTTON="Back"
fi
[ -f "${GCPAD_INI}" ] && sed -i "/^Buttons\/Hotkey =/c\Buttons\/Hotkey = \`${HOTKEY_BUTTON}\`" "${GCPAD_INI}"
if [ "${HOTKEY_BUTTON}" = "Guide" ]; then
  sed -i 's/`Back`+/`Guide`+/g' "${CFG_DIR}/Hotkeys.ini"
else
  sed -i 's/`Guide`+/`Back`+/g' "${CFG_DIR}/Hotkeys.ini"
fi

# SDL names a gamepad after the mapping it resolves, which is not always the
# joystick name control-gen reports.
SDL_DB="${SDL_GAMECONTROLLERCONFIG_FILE:-/storage/.config/gptokeyb/gamecontrollerdb.txt}"
SDL_DEVICE="${param_device}"
MAPLINE=""
if [ -n "${DEVICE}" ] && [ -f "${SDL_DB}" ]; then
  GUID_KEY="$(echo "${DEVICE}" | cut -c1-4)0000$(echo "${DEVICE}" | cut -c9-)"
  MAPLINE="$(awk -F, -v d="${DEVICE}" -v k="${GUID_KEY}" '!/^#/ && NF>1 {
      if ($1 == d) { exact = $0; exit }
      if (loose == "" && substr($1,5,4) == "0000") {
        g = substr($1,1,4) "0000" substr($1,9)
        if (g == k) { loose = $0 }
      }
    }
    END { print (exact != "" ? exact : loose) }' "${SDL_DB}")"
  MAPPED="$(echo "${MAPLINE}" | cut -d, -f2)"
  [ -n "${MAPPED}" ] && SDL_DEVICE="${MAPPED}"
fi

# Some SDL mappings are Xbox-style although the pad is physically Nintendo-style
FACE_SWAP="no"
if [ -n "${MAPLINE}" ]; then
  A_BTN="$(echo "${MAPLINE}" | tr ',' '\n' | sed -n 's/^a:b\([0-9]\{1,\}\)$/\1/p')"
  B_BTN="$(echo "${MAPLINE}" | tr ',' '\n' | sed -n 's/^b:b\([0-9]\{1,\}\)$/\1/p')"
  if [ -n "${A_BTN}" ] && [ -n "${B_BTN}" ] && [ "${A_BTN}" -gt "${B_BTN}" ]; then
    FACE_SWAP="yes"
  fi
fi

for CFG in "${CFG_DIR}/Hotkeys.ini" "${GCPAD_INI}" "${WIIMOTE_INI}"; do
  [ -f "${CFG}" ] || continue
  sed -i "/^Device = /c\\Device = SDL/0/${SDL_DEVICE}" "${CFG}"

  # Only freshly copied files are written positionally and need correcting
  if [ "${FACE_SWAP}" = "yes" ] && \
     ! { [ "${CON}" = "custom" ] && [ "${CFG}" = "${WIIMOTE_INI}" ]; } && \
     ! { [ "${WPC}" = "false" ] && [ "${CFG}" = "${CFG_DIR}/Hotkeys.ini" ]; }; then
    sed -i -e 's/`Button S`/`Button %`/g' \
           -e 's/`Button E`/`Button S`/g' \
           -e 's/`Button %`/`Button E`/g' \
           -e 's/`Button W`/`Button %`/g' \
           -e 's/`Button N`/`Button W`/g' \
           -e 's/`Button %`/`Button N`/g' "${CFG}"
  fi
done

if [ "${PRIMEHACK_BIN}" = "primehack" ]; then
  export QT_QPA_PLATFORM=wayland
  ARGS="-b"
else
  ARGS="-p wayland"
fi

echo "GAME set to: ${GAME}"
echo "PLATFORM set to: ${PLATFORM}"
echo "CPU CORES set to: ${EMUPERF}"
echo "ASPECT set to: ${ASPECT}"
echo "GRENDERER set to: ${GRENDERER}"
echo "IRES set to: ${IRES}"
echo "FPS set to: ${FPS}"
echo "CON set to: ${CON}"
echo "HKEY set to: ${HKEY}"
echo "SHADERM set to: ${SHADERM}"
echo "VSYNC set to: ${VSYNC}"
echo "RUMBLE set to: ${RUMBLE}"
echo "WPC set to: ${WPC}"
echo "Launching /usr/bin/${PRIMEHACK_BIN} -u ${CONF_DIR} ${ARGS} -e ${1}"

${EMUPERF} /usr/bin/${PRIMEHACK_BIN} -u "${CONF_DIR}" ${ARGS} -e "${1}"
