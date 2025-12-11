#!/bin/bash
set -euo pipefail

ARCH=${1:-x86_64}
ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
LIBC_DIR="$ROOT_DIR/libc"
APPS_DIR="$ROOT_DIR/apps"
PREBUILT_DIR="$ROOT_DIR/prebuilt/$ARCH"
ROOTFS_DIR="$ROOT_DIR/rootfs/$ARCH"

mkdir -p "$PREBUILT_DIR" "$ROOTFS_DIR"

pushd "$LIBC_DIR" >/dev/null
make ARCH=$ARCH
popd >/dev/null

pushd "$APPS_DIR" >/dev/null
make ARCH=$ARCH LIBC_DIR="$LIBC_DIR/build/$ARCH"
popd >/dev/null

cp "$APPS_DIR/build/$ARCH"/*.elf "$PREBUILT_DIR"/

rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"/sbin "$ROOTFS_DIR"/bin "$ROOTFS_DIR"/usr/bin
cp "$PREBUILT_DIR"/init.elf "$ROOTFS_DIR"/sbin/init
cp "$PREBUILT_DIR"/shell.elf "$ROOTFS_DIR"/bin/sh
cp "$PREBUILT_DIR"/cat.elf "$ROOTFS_DIR"/bin/cat
cp "$PREBUILT_DIR"/ls.elf "$ROOTFS_DIR"/bin/ls
cp "$PREBUILT_DIR"/stress.elf "$ROOTFS_DIR"/usr/bin/stress

IMAGE_DIR="$ROOT_DIR/build/$ARCH"
mkdir -p "$IMAGE_DIR"
IMAGE_PATH="$IMAGE_DIR/rootfs.ext2"

if command -v genext2fs >/dev/null 2>&1; then
    genext2fs -d "$ROOTFS_DIR" -b 65536 "$IMAGE_PATH"
else
    tar -C "$ROOTFS_DIR" -cf "$IMAGE_DIR/rootfs.tar" .
fi

echo "Userspace binaries for $ARCH are in $PREBUILT_DIR"
