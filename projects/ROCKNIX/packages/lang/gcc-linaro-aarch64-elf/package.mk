PKG_NAME="gcc-linaro-aarch64-elf"
PKG_LICENSE="GPL"
PKG_DEPENDS_HOST="ccache:host"
PKG_TOOLCHAIN="manual"

PKG_VERSION="15.2.rel1"
PKG_SITE="https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
PKG_LONGDESC="ARM GNU AArch64 bare-metal cross toolchain (aarch64-none-elf)"

if [ "$(uname -m)" = "aarch64" ]; then
  PKG_URL="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-elf.tar.xz"
  PKG_SHA256="46195685b6aec1077e3f1b7706b43a6aa1fef4d8d3bff3a411b7dad1c5b1196b"
else
  PKG_URL="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf.tar.xz"
  PKG_SHA256="66f7ce7c1bf662f589a4caf440812375f3cd8000a033ccf0971127a0726d6921"
fi

makeinstall_host() {
  mkdir -p $TOOLCHAIN/lib/gcc-linaro-aarch64-elf/
    cp -a * $TOOLCHAIN/lib/gcc-linaro-aarch64-elf
}
