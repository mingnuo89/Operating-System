#!/usr/bin/env bash
set -euo pipefail
# Run inside the course container. Dependencies are listed in README.md.
lab_dir="$(cd "$(dirname "$0")/.." && pwd)"
qemu_build_base="${QEMU_BUILD_BASE:-/tmp/lab2-qemu}"
mkdir -p "$lab_dir/.debug" "$qemu_build_base"
archive="$lab_dir/.debug/qemu-4.1.1.tar.xz"
if [[ ! -f "$archive" ]]; then
    curl --fail --location https://download.qemu.org/qemu-4.1.1.tar.xz --output "$archive"
fi
if [[ ! -d "$qemu_build_base/qemu-4.1.1" ]]; then
    tar -xJf "$archive" -C "$qemu_build_base"
fi
cd "$qemu_build_base/qemu-4.1.1"
./configure --target-list=riscv64-softmmu --enable-debug --disable-werror \
    --disable-docs --disable-gtk --disable-sdl --disable-vnc --disable-curses \
    --disable-tools > "$lab_dir/.debug/configure.log" 2>&1
make -j"${QEMU_BUILD_JOBS:-6}" > "$lab_dir/.debug/build.log" 2>&1
./riscv64-softmmu/qemu-system-riscv64 --version
