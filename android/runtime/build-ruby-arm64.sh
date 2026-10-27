#!/usr/bin/env bash
set -euo pipefail

# Build only the Ruby VM; this does not produce a runnable Android game.
if [ "$#" -ne 3 ]; then
  echo "Usage: bash build-ruby-arm64.sh RUBY_TAR_GZ ANDROID_NDK BUILD_DIR" >&2
  exit 2
fi
archive=$(realpath "$1")
ndk=$(realpath "$2")
out=$3
if [[ "$ndk" == *' '* ]]; then
  echo "Use an NDK path without spaces; Ruby configure treats CC as a shell command." >&2
  exit 2
fi
if [ -e "$out" ]; then
  echo "Build directory already exists: $out" >&2
  exit 2
fi
expected=911ace20f90d068ca0e4dda6d0e4f0f81e52e52f2dd4f4004c721e253412e82d
actual=$(sha256sum "$archive" | cut -d' ' -f1)
if [ "$actual" != "$expected" ]; then
  echo 'Ruby 4.0.7 archive SHA-256 mismatch.' >&2
  exit 2
fi
ruby -rerb -e 'exit(RUBY_VERSION.to_f >= 3.1 ? 0 : 1)'
toolchain="$ndk/toolchains/llvm/prebuilt/windows-x86_64"
for tool in clang.exe clang++.exe llvm-ar.exe llvm-ranlib.exe llvm-strip.exe; do
  test -f "$toolchain/bin/$tool" || { echo "Missing NDK tool: $tool" >&2; exit 2; }
done
mkdir -p "$out/source" "$out/build"
out=$(realpath "$out")
tar -xzf "$archive" -C "$out/source" --strip-components=1
cd "$out/build"
export CC="$toolchain/bin/clang.exe --target=aarch64-linux-android26 --sysroot=$toolchain/sysroot"
export CXX="$toolchain/bin/clang++.exe --target=aarch64-linux-android26 --sysroot=$toolchain/sysroot"
export AR="$toolchain/bin/llvm-ar.exe"
export RANLIB="$toolchain/bin/llvm-ranlib.exe"
export STRIP="$toolchain/bin/llvm-strip.exe"
export CFLAGS=-fPIC
"$out/source/configure" --build=x86_64-pc-msys --host=aarch64-linux-android \
  --disable-install-doc --disable-yjit --disable-zjit --without-gmp \
  --disable-shared --disable-pie --disable-rubygems --with-baseruby="$(command -v ruby)"
make -j4 libruby-static.a
ruby "$out/source/tool/generic_erb.rb" -c -o "$out/build/encdb.h" "$out/source/template/encdb.h.tmpl" "$out/source/enc"
ruby "$out/source/tool/generic_erb.rb" -c -o "$out/build/transdb.h" "$out/source/template/transdb.h.tmpl" "$out/source/enc/trans"
echo "Ruby VM archive: $out/build/libruby-static.a"
