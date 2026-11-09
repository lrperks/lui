# Lui build.
#
#   make            build bin/luic and bin/lui_host
#   make test       compile every example and compare with tests/expected
#   make clean      remove build output
#
# Needs a C/C++ compiler, Lua 5.3 (pkg-config lua5.3) and raylib.  Dear ImGui
# and rlImGui are git submodules: git submodule update --init

RAYLIB_PREFIX ?= $(HOME)/.local
PKG_CONFIG_PATH := $(RAYLIB_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

CC  ?= gcc
CXX ?= g++
CFLAGS   ?= -O2
CXXFLAGS ?= -O2

LUA_CFLAGS := $(shell pkg-config --cflags lua5.3)
LUA_LIBS   := $(shell pkg-config --libs lua5.3)
RAYLIB_INC := -I$(RAYLIB_PREFIX)/include
LIBS       := -L$(RAYLIB_PREFIX)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

IMGUI   := third_party/imgui
RLIMGUI := third_party/rlImGui
GUI_INC := -I$(IMGUI) -I$(RLIMGUI) $(RAYLIB_INC)
GUI_SRC := $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp $(IMGUI)/imgui_tables.cpp \
           $(IMGUI)/imgui_widgets.cpp $(RLIMGUI)/rlImGui.cpp
GUI_OBJ := $(patsubst %.cpp,build/%.o,$(notdir $(GUI_SRC)))

all: bin/luic bin/lui_host

bin/luic: src/luic.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -Wall -Wextra -o $@ $<

bin/lui_host: build/lui_host.o build/lui_imgui.o $(GUI_OBJ)
	@mkdir -p bin
	$(CXX) -o $@ $^ $(LUA_LIBS) $(LIBS)

build/lui_host.o: src/lui_host.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $< $(LUA_CFLAGS) $(RAYLIB_INC)

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

clean:
	rm -rf bin build

.PHONY: all test clean
