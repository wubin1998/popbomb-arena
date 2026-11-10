#!/bin/bash
# Lays out the PortMaster zip. Usage: assemble_port.sh <sdl-prefix>
# Run from the repository root.
set -eux

sdl_root=$1
out=build/port

rm -rf "$out" build/popbomb.zip
mkdir -p "$out/popbomb/data/gfx" "$out/popbomb/data/romfs/audio" \
         "$out/popbomb/libs.aarch64"

cp ports/r36s/PopBomb.sh "$out/"
cp ports/r36s/popbomb/port.json ports/r36s/popbomb/README.md \
   ports/r36s/popbomb/gameinfo.xml ports/r36s/popbomb/screenshot.png \
   ports/r36s/popbomb/popbomb.gptk "$out/popbomb/"
install -m 755 ports/sdl/popbomb-sdl "$out/popbomb/popbomb-sdl.aarch64"
cp -L "$sdl_root/lib/libSDL2-2.0.so.0" "$out/popbomb/libs.aarch64/"
cp ports/3ds/gfx/*.png ports/3ds/gfx/*.t3s "$out/popbomb/data/gfx/"
cp ports/3ds/romfs/audio/*.wav "$out/popbomb/data/romfs/audio/"

ldd "$out/popbomb/popbomb-sdl.aarch64"
(cd "$out" && zip -qr ../popbomb.zip .)
ls -lh build/popbomb.zip
