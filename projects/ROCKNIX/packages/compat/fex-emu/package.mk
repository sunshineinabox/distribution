# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="fex-emu"
PKG_VERSION="e869aa644a16e4332cdc15c1ea0b4d13d482385d"
PKG_LICENSE="MIT"
PKG_SITE="https://github.com/FEX-Emu/FEX"
PKG_URL="https://github.com/FEX-Emu/FEX.git"
PKG_DEPENDS_TARGET="toolchain llvm:host fex-emu:host mesa:host squashfs-tools zlib squashfuse alsa-lib libxcb wayland libglvnd libdrm libX11 libXrandr xorgproto qt6"
PKG_DEPENDS_HOST="toolchain:host llvm:host openssl:host"
# reuse mesa's release tarball: on aarch64 build hosts make_target
# cross-compiles the x86_64 guest turnip driver from it, and the unpack
# dependency keeps this package's stamp in lockstep with mesa bumps
PKG_DEPENDS_UNPACK+=" mesa"
PKG_LONGDESC="FEX-Emu is a fast x86/x86-64 emulator for AArch64"
PKG_TOOLCHAIN="manual"

FEX_LLVM_BIN="${TOOLCHAIN}/bin"
FEX_CLANG="${FEX_LLVM_BIN}/clang"
FEX_CLANGXX="${FEX_LLVM_BIN}/clang++"
FEX_CMAKE_BASE=(
  -DCMAKE_BUILD_TYPE=Release
  -DENABLE_LTO=True
  -DBUILD_TESTING=False
  -DBUILD_THUNKS=True
  -DCMAKE_INSTALL_PREFIX=/usr
  -DCMAKE_MAKE_PROGRAM=ninja
  -DCMAKE_C_COMPILER="${FEX_CLANG}"
  -DCMAKE_CXX_COMPILER="${FEX_CLANGXX}"
  
  # Make sure we pick up teh right llvm-ar and llvm-ranlib
  -DCMAKE_AR="${FEX_LLVM_BIN}/llvm-ar"
  -DCMAKE_RANLIB="${FEX_LLVM_BIN}/llvm-ranlib"
  -DCMAKE_C_COMPILER_AR="${FEX_LLVM_BIN}/llvm-ar"
  -DCMAKE_CXX_COMPILER_AR="${FEX_LLVM_BIN}/llvm-ar"
  -DCMAKE_ASM_COMPILER_AR="${FEX_LLVM_BIN}/llvm-ar"
  -DCMAKE_C_COMPILER_RANLIB="${FEX_LLVM_BIN}/llvm-ranlib"
  -DCMAKE_CXX_COMPILER_RANLIB="${FEX_LLVM_BIN}/llvm-ranlib"
  -DCMAKE_ASM_COMPILER_RANLIB="${FEX_LLVM_BIN}/llvm-ranlib"
)

FEX_CMAKE_OPTS=(
  "${FEX_CMAKE_BASE[@]}"
  -DUSE_LINKER=lld
  -DENABLE_ASSERTIONS=False
  -DCMAKE_LINKER="${FEX_LLVM_BIN}/ld.lld"
)

make_host() {
  mkdir -p "${PKG_BUILD}/.${HOST_NAME}"
  cd "${PKG_BUILD}"

  local -a host_opts=(
    -G Ninja
    -S "${PKG_BUILD}"
    -B "${PKG_BUILD}/.${HOST_NAME}"
    "${FEX_CMAKE_BASE[@]}"
    -DUSE_LINKER="${FEX_LLVM_BIN}/ld.lld"
    -DBUILD_FEXCONFIG=False
    -DTHUNKGEN_ONLY=True
    -DCMAKE_ASM_COMPILER="${FEX_CLANG}"
    -DCMAKE_PREFIX_PATH="${TOOLCHAIN}"
    -DCLANG_EXEC_PATH="${FEX_CLANG}"
    -DENABLE_X86_HOST_DEBUG=True
  )
  cmake "${host_opts[@]}"
  cd "${PKG_BUILD}/.${HOST_NAME}"
  ninja thunkgen
}

make_target() {
  local _v
  for _v in CFLAGS CXXFLAGS LDFLAGS; do
    export ${_v}="$(echo ${!_v} | sed 's/-mabi=lp64//g; s/-mtune=[^ ]*//g')"
  done
  export USER="${USER:-$(whoami)}"
  export HOME=${PKG_BUILD}/nix
  curl -L https://nixos.org/nix/install | sh -s -- --no-daemon
  . "${HOME}/.nix-profile/etc/profile.d/nix.sh"
  # Pin nixpkgs: the thunk toolchain and x86 dev rootfs come from
  # <nixpkgs>; unpinned, they roll with the channel on every build and
  # header drift breaks the generated thunks (nixos-unstable 2026-08-05)
  export NIX_PATH="nixpkgs=https://github.com/NixOS/nixpkgs/archive/ee67c8504dafc87ba63e862d76558384d10e1e8c.tar.gz"

  mkdir -p "${PKG_BUILD}/.${TARGET_NAME}"
  cd "${PKG_BUILD}/.${TARGET_NAME}"

  case ${TARGET_CPU} in
    cortex-x3|cortex-x4)
      TUNE_CPU="cortex-a78"
      ;;
    *)
      TUNE_CPU="${TARGET_CPU##*.}"
      ;;
  esac

  # thunkgen host-parse system headers. --sysroot instead of -isystem so the
  # guest parse's own --sysroot (appended later, last one wins) overrides it;
  # libstdc++ passed explicitly since it is not discoverable under a sysroot.
  local cxxdir
  cxxdir=$(ls -d "${TOOLCHAIN}/${TARGET_NAME}/include/c++/"* | sort -V | tail -n1)
  # --target pins the host parse to the device triple: on an aarch64 build
  # host clang's default target already matches, but on an x86_64 build host
  # the host parse otherwise reads the aarch64 sysroot as x86_64 - SVE types
  # in bits/math-vector.h are unknown and every host_layout is generated with
  # the wrong ABI, which the .inl compiles then reject.
  export THUNKGEN_EXTRA_FLAGS="--target=${TARGET_NAME} --sysroot ${SYSROOT_PREFIX} -isystem ${cxxdir} -isystem ${cxxdir}/${TARGET_NAME}"

  local -a tgt_opts=(
    -G Ninja
    -S "${PKG_BUILD}"
    -B "${PKG_BUILD}/.${TARGET_NAME}"
    -DCMAKE_SYSTEM_NAME=Linux
    -DCMAKE_SYSTEM_PROCESSOR=aarch64
    -DCMAKE_C_COMPILER_TARGET=aarch64-rocknix-linux-gnu
    -DCMAKE_CXX_COMPILER_TARGET=aarch64-rocknix-linux-gnu
    -DCMAKE_SYSROOT="${SYSROOT_PREFIX}"
    -DCMAKE_FIND_ROOT_PATH="${SYSROOT_PREFIX}"
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
    -DBUILD_FEXCONFIG=True
    "${FEX_CMAKE_OPTS[@]}"
    -DGENERATOR_EXE="${TOOLCHAIN}/usr/bin/thunkgen"
    -DCMAKE_INSTALL_LIBDIR=lib
    -DQT_HOST_PATH="${TOOLCHAIN}/usr/local/qt6"
    -DTUNE_CPU="${TUNE_CPU}"
  )
  cmake "${tgt_opts[@]}"
  # aarch64 build host: x86_64 thunk descriptor needs the cross prefix, not bare clang
  if [ "$(uname -m)" = "aarch64" ]; then
    sed -i 's#/bin/clang)#/bin/x86_64-unknown-linux-gnu-clang)#; s#/bin/clang++)#/bin/x86_64-unknown-linux-gnu-clang++)#' "${PKG_BUILD}/Data/nix/LibraryForwarding/shell.nix"
  fi
  bash "${PKG_BUILD}/Data/nix/cmake_enable_libfwd.sh"
  ninja

  # Guest x86_64 Vulkan driver (turnip): on an aarch64 build host the
  # mesa:host pass produced an aarch64 libvulkan_freedreno.so, which the
  # x86_64 FEX guest rootfs cannot load. Cross-compile the same mesa
  # release for x86_64 with the pinned nix environment the thunks
  # already use, so both builder architectures ship the same driver.
  if [ "$(uname -m)" = "aarch64" ]; then
    local mesa_version mesa_tarball vk_out
    mesa_version="$(get_pkg_version mesa)"
    mesa_tarball="${SOURCES}/mesa/$(get_pkg_variable mesa PKG_SOURCE_NAME)"
    [ -f "${mesa_tarball}" ] || \
      die "FEX guest vulkan: mesa source ${mesa_tarball} not found (should have been fetched via PKG_DEPENDS_UNPACK mesa)"
    vk_out="${PKG_BUILD}/.${TARGET_NAME}/fex-guest-vulkan"
    rm -rf "${vk_out}"
    mkdir -p "${vk_out}"
    nix-build "${PKG_DIR}/nix/guest-vulkan-freedreno.nix" \
      --argstr mesaTarball "${mesa_tarball}" \
      --argstr mesaVersion "${mesa_version}" \
      --out-link "${vk_out}/result" || \
      die "FEX guest vulkan: cross build of x86_64 libvulkan_freedreno.so failed"
    install -m 0644 "${vk_out}/result/lib/libvulkan_freedreno.so" \
      "${vk_out}/libvulkan_freedreno.so"
  fi
}

makeinstall_target() {
  cd "${PKG_BUILD}/.${TARGET_NAME}"
  DESTDIR="${INSTALL}" ninja install
  mkdir -p "${INSTALL}/usr/config/fex-emu"
  cp -rf "${PKG_DIR}/config/fex-emu/." "${INSTALL}/usr/config/fex-emu"
  cp -rf "${PKG_DIR}/config/gptk" "${INSTALL}/usr/config/fex-emu"
  mkdir -p "${INSTALL}/usr/config/modules"
  cp -rf "${PKG_DIR}/scripts/"* "${INSTALL}/usr/config/modules"
  # Install Steam.sh drops this into the ArchLinux guest rootfs, which is
  # x86_64, and relies on it being in the image, so an x86_64 driver must
  # always ship. One producer per build-host architecture:
  #   x86_64 host:  the mesa:host pass built an x86_64 libvulkan_freedreno.so
  #   aarch64 host: make_target cross-compiled it with the pinned nix env
  # The machine check is now a guarantee: fail the build rather than ship
  # an image whose Steam has no Vulkan driver.
  local vk
  if [ "$(uname -m)" = "aarch64" ]; then
    vk="${PKG_BUILD}/.${TARGET_NAME}/fex-guest-vulkan/libvulkan_freedreno.so"
  else
    vk="${TOOLCHAIN}/lib/libvulkan_freedreno.so"
  fi
  local vk_machine="$(readelf -h "${vk}" 2>/dev/null | sed -n 's/^ *Machine: *//p')"
  case "${vk_machine}" in
    *X86-64*|*x86-64*)
      mkdir -p "${INSTALL}/usr/share/fex-emu"
      cp "${vk}" "${INSTALL}/usr/share/fex-emu/"
      ;;
    *)
      die "FEX guest vulkan: ${vk} is ${vk_machine:-missing/unreadable}, expected x86-64.\nThe FEX guest rootfs is x86_64 and Install Steam.sh depends on this driver;\nrefusing to produce an image without it."
      ;;
  esac
}

makeinstall_host() {
  mkdir -p "${TOOLCHAIN}/usr/bin"
  cp -av "${PKG_BUILD}/.${HOST_NAME}/Bin/thunkgen" "${TOOLCHAIN}/usr/bin"
}
