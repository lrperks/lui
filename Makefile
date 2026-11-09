# Lui build.  `make` builds bin/luic and bin/lui_host.
#
#   make             build everything
#   make clean       remove build output
#
# lui_host needs raylib, Lua 5.3 and the cimgui/rlImGui build in third_party/build.

RAYLIB_PREFIX ?= $(HOME)/.local

PKG_CONFIG_PATH := $(RAYLIB_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

LUA_CFLAGS := $(shell pkg-config --cflags lua5.3)
LUA_LIBS   := $(shell pkg-config --libs lua5.3)
IMGUI_INC  := third_party/cimgui/imgui third_party/rlImGui
LIBS       := -L$(RAYLIB_PREFIX)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lstdc++

all: bin/luic bin/lui_host

bin/luic: src/luic.c
	@mkdir -p bin
	gcc -O2 -Wall -Wextra -o $@ $<

bin/lui_host: src/lui_host.o src/lui_imgui.o third_party/build/libcimgui_rl.a
	@mkdir -p bin
	g++ -o $@ src/lui_host.o src/lui_imgui.o third_party/build/libcimgui_rl.a $(LUA_LIBS) $(LIBS)

src/lui_host.o: src/lui_host.c
	gcc -O2 -Wall -c -o $@ $< $(LUA_CFLAGS) -I$(RAYLIB_PREFIX)/include

src/lui_imgui.o: src/lui_imgui.cpp
	g++ -O2 -Wall -c -o $@ $< $(LUA_CFLAGS) $(addprefix -I,$(IMGUI_INC)) -I$(RAYLIB_PREFIX)/include

clean:
	rm -rf bin
	rm -f src/*.o

.PHONY: all clean
