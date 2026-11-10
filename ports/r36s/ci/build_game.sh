#!/bin/bash
# Builds the SDL2 build of the game. Usage: build_game.sh <sdl-prefix>
set -eux

sdl_root=$1

make -C ports/sdl -j"$(getconf _NPROCESSORS_ONLN)" \
  SDL_CFLAGS="-I$sdl_root/include/SDL2 -D_THREAD_SAFE" \
  SDL_LIBS="-L$sdl_root/lib -lSDL2"
