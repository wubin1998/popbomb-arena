# SDL2 skeleton for the R36S port.
# Reuses the 3DS game logic and renderer unchanged; only the platform layer
# (shim/citro2d.c, audio_sdl.c, sdl_main.c) is new.

SDL_CFLAGS ?= $(shell sdl2-config --cflags)
SDL_LIBS   ?= $(shell sdl2-config --libs)

SRC3DS := ../3ds/source

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wno-unused-parameter -I. -Ishim -I$(SRC3DS) -Ithird_party $(SDL_CFLAGS)
LIBS   := $(SDL_LIBS) -lm

OBJS := sdl_main.o audio_sdl.o citro2d.o \
        map.o hero.o bomb.o game.o rng.o tools.o render.o font.o assets.o

VPATH := . shim $(SRC3DS)

all: popbomb-sdl

popbomb-sdl: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) popbomb-sdl

.PHONY: all clean
