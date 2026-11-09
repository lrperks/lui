# Lui build.
#
#   make                    build bin/lui (and bin/luic, the bare compiler)
#   make test               compile every example and compare with tests/expected
#   make install            install lui to $(PREFIX)/bin (default /usr/local)
#   make install PREFIX=~/.local
#   make clean              remove build output
#
# Needs a C/C++ compiler, Lua 5.3 or 5.4, and raylib (./install.sh sets all of
# this up).  Dear ImGui and rlImGui are git submodules: git submodule update --init

RAYLIB_PREFIX ?= $(HOME)/.local
PKG_CONFIG_PATH := $(RAYLIB_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

CC  ?= gcc
CXX ?= g++
CFLAGS   ?= -O2
CXXFLAGS ?= -O2

# Lua 5.3 or 5.4, under whichever name the distribution uses.
LUA_PC     := $(firstword $(foreach p,lua5.4 lua-5.4 lua54 lua5.3 lua-5.3 lua53 lua,\
                  $(shell pkg-config --exists $(p) && echo $(p))))
LUA_CFLAGS := $(shell pkg-config --cflags $(LUA_PC))
LUA_LIBS   := $(shell pkg-config --libs $(LUA_PC))
RAYLIB_INC := -I$(RAYLIB_PREFIX)/include
LIBS       := -L$(RAYLIB_PREFIX)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

IMGUI   := third_party/imgui
RLIMGUI := third_party/rlImGui
GUI_INC := -I$(IMGUI) -I$(RLIMGUI) $(RAYLIB_INC)
GUI_SRC := $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp $(IMGUI)/imgui_tables.cpp \
           $(IMGUI)/imgui_widgets.cpp $(RLIMGUI)/rlImGui.cpp
GUI_OBJ := $(patsubst %.cpp,build/%.o,$(notdir $(GUI_SRC)))

PREFIX ?= /usr/local

all: bin/lui bin/luic

bin/lui: build/main.o build/luic.o build/runtime.o build/lui_imgui.o $(GUI_OBJ)
	@mkdir -p bin
	$(CXX) -o $@ $^ $(LUA_LIBS) $(LIBS)

bin/luic: src/luic.c src/luic.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -Wall -Wextra -DLUIC_MAIN -o $@ src/luic.c

build/main.o: src/main.c src/luic.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $< $(LUA_CFLAGS) $(RAYLIB_INC)

build/luic.o: src/luic.c src/luic.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

# The runtime is embedded in the binary (see src/runtime.S).
build/runtime.o: src/runtime.S src/lui.lua
	@mkdir -p build
	$(CC) -c -o $@ src/runtime.S

build/lui_imgui.o: src/lui_imgui.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Wall -c -o $@ $< $(LUA_CFLAGS) $(GUI_INC)

# Third-party sources: built once, without our warning flags.
build/%.o: $(IMGUI)/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(GUI_INC)

build/%.o: $(RLIMGUI)/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(GUI_INC)

test: bin/luic
	@sh tests/run.sh

install: bin/lui
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 bin/lui $(DESTDIR)$(PREFIX)/bin/lui

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/lui

clean:
	rm -rf bin build

.PHONY: all test install uninstall clean
