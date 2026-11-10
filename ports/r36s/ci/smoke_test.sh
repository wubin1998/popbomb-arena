#!/bin/bash
# Headless run of the built binary. Usage: smoke_test.sh <sdl-prefix>
# Run from the repository root.
set -eux

sdl_root=$1

LD_LIBRARY_PATH="$sdl_root/lib" \
POPBOMB_SELFTEST=300 POPBOMB_NOAUDIO=1 SDL_VIDEODRIVER=dummy \
POPBOMB_DATA="$PWD/ports/3ds" ./ports/sdl/popbomb-sdl

file ports/sdl/popbomb-sdl
