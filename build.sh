#!/bin/sh
# Builds the plugin, out/camera_distance.nro.
#
#   sh build.sh            build only
#   sh build.sh install    build, then install the mod into the recomp's mods/camera-distance/
#   sh build.sh release    build, then make dist/camera-distance-<version>.zip, the folder players put in mods/
#
# install needs the recomp's folder: RECOMP=<path> (the folder holding data/ and mods/; on macOS
# ~/Library/Application Support/MHGURecomp, the default).
#
# Needs an arm-none-eabi GCC and binutils (Homebrew's arm-none-eabi-gcc; CROSS=<prefix> for another) and Python 3.
set -e
cd "$(dirname "$0")"
VERSION=0.1.0
CROSS=${CROSS:-arm-none-eabi-}
RECOMP=${RECOMP:-"$HOME/Library/Application Support/MHGURecomp"}
OUT=out
mkdir -p "$OUT"

# Hard float: the game returns floats in VFP registers (the camera's numbers). VFP only, no NEON.
CFLAGS="-march=armv7-a -marm -mfloat-abi=hard -mfpu=vfpv3-d16 -std=gnu11 -O2 -fPIC -ffreestanding -fno-builtin -nostdlib
        -fno-stack-protector -fvisibility=hidden -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wextra
        -DPLUGIN_VERSION=\"$VERSION\""
"${CROSS}gcc" $CFLAGS -c src/plugin.c -o "$OUT/plugin.o"
"${CROSS}gcc" $CFLAGS -c src/start.S -o "$OUT/start.o"
"${CROSS}ld" -shared -T build/nro.ld --hash-style=sysv -z max-page-size=0x1000 \
    -soname camera_distance.nro "$OUT/start.o" "$OUT/plugin.o" -o "$OUT/camera_distance.elf"
python3 -I build/elf2nro.py "$OUT/camera_distance.elf" "$OUT/camera_distance.nro"

# The mod's folder as players get it: the plugin and a short readme
make_mod() {
    mkdir -p "$1/romfs/nativeNX/plugins"
    cp "$OUT/camera_distance.nro" "$1/romfs/nativeNX/plugins/"
    sed "s/@VERSION@/$VERSION/" release/README.txt > "$1/README.txt"
}

case "$1" in
install)
    make_mod "$RECOMP/mods/camera-distance"
    echo "installed: $RECOMP/mods/camera-distance"
    ;;
release)
    rm -rf "$OUT/release" && mkdir -p "$OUT/release" dist
    make_mod "$OUT/release/camera-distance"
    rm -f "dist/camera-distance-$VERSION.zip"
    (cd "$OUT/release" && zip -qrX "../../dist/camera-distance-$VERSION.zip" camera-distance)
    echo "made: dist/camera-distance-$VERSION.zip"
    ;;
esac
