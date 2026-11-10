#!/bin/sh
# Installs lui: its dependencies, raylib (if missing), and the `lui` command.
#
#   ./install.sh                     install to ~/.local/bin
#   ./install.sh --prefix /usr/local
#   ./install.sh --no-deps           skip the system package step
set -eu

PREFIX="$HOME/.local"
DEPS=1
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --prefix=*) PREFIX="${1#*=}"; shift ;;
        --no-deps) DEPS=0; shift ;;
        -h|--help) sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

cd "$(dirname "$0")"
say() { printf '\033[1m==> %s\033[0m\n' "$*"; }

# ── system packages ────────────────────────────────────────────────────
if [ "$DEPS" = 1 ]; then
    SUDO=""
    [ "$(id -u)" -ne 0 ] && SUDO="sudo"
    if command -v apt-get >/dev/null; then
        say "Installing packages (apt)"
        $SUDO apt-get install -y build-essential cmake git pkg-config liblua5.3-dev curl \
            fonts-dejavu-core libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
            libxi-dev libgl1-mesa-dev
    elif command -v dnf >/dev/null; then
        say "Installing packages (dnf)"
        $SUDO dnf install -y gcc gcc-c++ make cmake git pkgconf-pkg-config lua-devel curl \
            dejavu-sans-fonts libX11-devel libXrandr-devel libXinerama-devel \
            libXcursor-devel libXi-devel mesa-libGL-devel
    elif command -v pacman >/dev/null; then
        say "Installing packages (pacman)"
        $SUDO pacman -S --needed --noconfirm base-devel cmake git pkgconf lua curl ttf-dejavu \
            libx11 libxrandr libxinerama libxcursor libxi mesa raylib
    else
        echo "Unknown package manager: install a C/C++ compiler, cmake, git, pkg-config," >&2
        echo "Lua 5.3/5.4 headers, curl and the X11/OpenGL headers, then rerun with --no-deps." >&2
        exit 1
    fi
fi

# ── Dear ImGui and rlImGui ─────────────────────────────────────────────
if [ -d .git ] && [ ! -f third_party/imgui/imgui.h ]; then
    say "Fetching submodules"
    git submodule update --init --recursive
fi

# ── raylib ─────────────────────────────────────────────────────────────
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$HOME/.local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
if pkg-config --exists raylib; then
    RAYLIB_PREFIX=$(pkg-config --variable=prefix raylib)
else
    say "Building raylib 5.5 into $PREFIX"
    tmp=$(mktemp -d)
    git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$tmp/raylib"
    cmake -S "$tmp/raylib" -B "$tmp/raylib/build" -DBUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PREFIX"
    cmake --build "$tmp/raylib/build" -j
    cmake --install "$tmp/raylib/build"
    rm -rf "$tmp"
    RAYLIB_PREFIX="$PREFIX"
fi

# ── lui ────────────────────────────────────────────────────────────────
say "Building lui"
make -j RAYLIB_PREFIX="$RAYLIB_PREFIX"
make install PREFIX="$PREFIX"

say "Installed $PREFIX/bin/lui"
case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "Add it to your PATH:  export PATH=\"$PREFIX/bin:\$PATH\"" ;;
esac
echo "Try it:  lui new hello && lui watch hello.lui"
