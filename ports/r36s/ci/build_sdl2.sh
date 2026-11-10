#!/bin/bash
# Builds SDL2 for the handheld port, KMSDRM + GLES only: EmuELEC has no
# X11 or Wayland compositor. Usage: build_sdl2.sh <prefix> <version> <workdir>
set -eux

prefix=$1
version=$2
work=$3

mkdir -p "$work"
cd "$work"

# Focal ships curl 7.68, which predates --retry-all-errors.
curl -fsSL --retry 4 --retry-delay 3 -o "SDL2-$version.tar.gz" \
  "https://www.libsdl.org/release/SDL2-$version.tar.gz"
tar xzf "SDL2-$version.tar.gz"

cmake -S "SDL2-$version" -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF \
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=ON \
  -DSDL_OPENGL=OFF -DSDL_OPENGLES=ON \
  -DSDL_ALSA=ON -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF \
  -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
  -DSDL_DBUS=OFF -DSDL_IBUS=OFF \
  -DSDL_LIBUDEV=ON -DSDL_HIDAPI=ON
cmake --build build -j"$(getconf _NPROCESSORS_ONLN)"
cmake --install build

test -f "$prefix/include/SDL2/SDL.h"
"$prefix/bin/sdl2-config" --cflags
"$prefix/bin/sdl2-config" --version
