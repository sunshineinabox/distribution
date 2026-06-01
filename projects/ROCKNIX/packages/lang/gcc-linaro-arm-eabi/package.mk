PKG_NAME="gcc-linaro-arm-eabi"
PKG_LICENSE="GPL"
PKG_DEPENDS_HOST="ccache:host"
PKG_TOOLCHAIN="manual"

PKG_VERSION="15.2.rel1"
PKG_SITE="https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
PKG_LONGDESC="ARM GNU AArch32 bare-metal cross toolchain (arm-none-eabi)"

if [ "$(uname -m)" = "aarch64" ]; then
  PKG_URL="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-aarch64-arm-none-eabi.tar.xz"
  PKG_SHA256="d061559d814b205ed30c5b7c577c03317ec447ca51cd5a159d26b12a5bbeb20c"
else
  PKG_URL="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz"
  PKG_SHA256="597893282ac8c6ab1a4073977f2362990184599643b4c5ee34870a8215783a16"
fi

makeinstall_host() {
  mkdir -p $TOOLCHAIN/lib/gcc-linaro-arm-eabi/
  cp -a * $TOOLCHAIN/lib/gcc-linaro-arm-eabi
  # ARM GNU toolchain uses arm-none-eabi- prefix, but some u-boot components
  # (scp_task) hardcode arm-eabi-. Create symlinks for compatibility.
  for f in $TOOLCHAIN/lib/gcc-linaro-arm-eabi/bin/arm-none-eabi-*; do
    [ -f "$f" ] || continue
    base="${f##*/}"
    suffix="${base#arm-none-eabi-}"
    ln -sf "$base" "$TOOLCHAIN/lib/gcc-linaro-arm-eabi/bin/arm-eabi-$suffix"
  done
}
