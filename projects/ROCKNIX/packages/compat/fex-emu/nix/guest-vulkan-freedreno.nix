# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 ROCKNIX (https://github.com/ROCKNIX)
#
# Cross-compile the x86_64 freedreno Vulkan driver (turnip) that
# 'Install Steam.sh' drops into the FEX x86_64 ArchLinux guest rootfs.
#
# Only used when the build host is aarch64: there the mesa:host pass
# produces an aarch64 libvulkan_freedreno.so the guest cannot load, so
# make_target builds this instead. On x86_64 build hosts the mesa:host
# driver is already the right machine and ships as-is.
#
# <nixpkgs> resolves through the NIX_PATH pin exported by package.mk -
# the same pinned tree the thunk toolchain (Data/nix/LibraryForwarding/
# shell.nix) builds from - and pkgsCross.gnu64 is the same cross package
# set the x86_64 guest thunks compile with, so this adds no new channel
# or pin.
#
# The meson option set mirrors the mesa:host pass in
# projects/ROCKNIX/packages/graphics/mesa/package.mk (vulkan driver
# only, no window-system platforms, no GL, no LLVM) so both build-host
# architectures ship a driver with the same feature set built from the
# same mesa release tarball.

{ pkgs ? import <nixpkgs> { }
  # absolute path to the mesa release tarball shared with the mesa
  # package (--argstr mesaTarball ${SOURCES}/mesa/mesa-<version>.tar.xz)
, mesaTarball
, mesaVersion ? "unknown"
}:

let
  cross = pkgs.pkgsCross.gnu64;

  # build-machine python for mesa's code generators; mesa's meson checks
  # for mako, packaging (mako version probe) and yaml unconditionally
  buildPython = cross.buildPackages.python3.withPackages
    (ps: with ps; [ mako packaging pyyaml ]);
in
cross.stdenv.mkDerivation {
  pname = "fex-guest-vulkan-freedreno";
  version = mesaVersion;

  src = /. + mesaTarball;

  strictDeps = true;

  nativeBuildInputs = [
    cross.buildPackages.meson
    cross.buildPackages.ninja
    cross.buildPackages.pkg-config
    # ir3 assembler grammar (src/freedreno/ir3) needs flex/bison even in
    # a turnip-only build
    cross.buildPackages.bison
    cross.buildPackages.flex
    buildPython
  ];

  buildInputs = [
    cross.libdrm
    cross.zlib
    cross.expat
  ];

  # the mesa:host pass builds freedreno with -fno-strict-aliasing; keep parity
  env.NIX_CFLAGS_COMPILE = "-fno-strict-aliasing";

  mesonFlags = [
    "-Dgallium-drivers="
    "-Dvulkan-drivers=freedreno"
    "-Dfreedreno-kmds=msm"
    "-Dplatforms="
    "-Dopengl=false"
    "-Dgles1=disabled"
    "-Dgles2=disabled"
    "-Degl=disabled"
    "-Dgbm=disabled"
    "-Dglx=disabled"
    "-Dglvnd=disabled"
    "-Dllvm=disabled"
    "-Dshared-llvm=disabled"
    "-Dvideo-codecs="
    "-Dtools="
    "-Dbuild-tests=false"
  ];
}
