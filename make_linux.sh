#!/bin/bash
# make_linux.sh — build a self-contained OpenTyrian binary on Linux.
#
# SDL3 and SDL3_net are built from source and linked STATICALLY.  A distro's
# libSDL3 is linked against whatever that distro has (libdecor, Wayland,
# PulseAudio, PipeWire...), and bundling it drags every one of those in as a
# hard dependency on the user's machine.  SDL built from source instead
# loads all of those backends with dlopen at run time, so the resulting
# binary needs nothing beyond glibc and libm - and whatever the user has
# (X11 or Wayland, ALSA or Pulse) gets picked up when it runs.
#
# The freeware Tyrian 2.1 data is fetched into ./data if missing.
#
# Build requirements (headers only; nothing is linked from them).  This is the
# SDL3 list from its docs/README-linux.md: SDL3 enables the X11, Wayland,
# PipeWire/PulseAudio/ALSA and other backends at compile time when their
# headers are present, then dlopen()s the libraries at run time.
#
# The X11 extension -dev packages below are hard requirements, not optional
# extras: SDL3 3.4.16's CheckX11 (cmake/sdlchecks.cmake) calls
# SDL_missing_dependency() for each enabled X11 extension, so configure stops
# with a FATAL_ERROR at the first one whose header/library is missing.  The
# mapping is:
#   libxcursor-dev  XCURSOR       libxext-dev  XDBE (Xdbe.h), XSYNC (sync.h),
#   libxi-dev       XINPUT                      XSHAPE (shape.h)
#   libxfixes-dev   XFIXES        libxrandr-dev XRANDR
#   libxss-dev      XSCRNSAVER    libxtst-dev   XTEST
# Everything else in the list only enables a dlopen()ed backend and is
# harmless (SDL just warns and disables it) if absent.
#   Debian/Ubuntu:  sudo apt install build-essential cmake pkg-config curl unzip \
#       libasound2-dev libpulse-dev libpipewire-0.3-dev libx11-dev libxext-dev \
#       libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
#       libxtst-dev libwayland-dev libxkbcommon-dev libdecor-0-dev \
#       libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libgbm-dev \
#       libdrm-dev libdbus-1-dev libudev-dev libusb-1.0-0-dev wayland-protocols
#
# Usage: ./make_linux.sh [--deps-only] [data-dir]
#   (default data dir: ./data, fetched if missing)
#   --deps-only builds just the static SDL3 prefix under build/sdl and exits;
#   CI uses it so the -Werror compile check links against the same SDL3 the
#   release does.
set -euo pipefail

cd "$(dirname "$0")"

DEPS_ONLY=false
if [ "${1:-}" = "--deps-only" ]; then
    DEPS_ONLY=true
    shift
fi
DATA_ARG="${1:-data}"

have_data() { find "$1" -maxdepth 1 -iname "tyrian1.lvl" 2>/dev/null | grep -q .; }

# ---- pinned dependencies, installed under build/sdl ----
SDL3_VER="3.4.16"
SDL3_NET_VER="3.2.0"
# SHA-256 of the official release tarballs (verified before anything is built).
SDL3_TARBALL_SHA256="7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68"
SDL3_NET_TARBALL_SHA256="098522fc26d4e302ef9348aee6e76e67fe504dfefd7f596236568f8330570c41"
PREFIX="$PWD/build/sdl"
NPROC="$(nproc 2>/dev/null || echo 4)"

verify_sha256() {  # $1 = file, $2 = expected hex digest
    if command -v sha256sum >/dev/null 2>&1; then
        echo "$2  $1" | sha256sum -c - >/dev/null
    else
        echo "$2  $1" | shasum -a 256 -c - >/dev/null
    fi
}

fetch_tarball() {  # $1 = name, $2 = version, $3 = url, $4 = sha256
    local name="$1" ver="$2" url="$3" sha="$4"
    local tgz="build/vendor/$name-$ver.tar.gz"
    if [ -f "$tgz" ]; then
        return 0
    fi
    echo "Downloading $name $ver ..."
    mkdir -p build/vendor
    curl -fL --progress-bar -o "$tgz" "$url"
    if ! verify_sha256 "$tgz" "$sha"; then
        echo "ERROR: checksum mismatch for $name $ver (expected $sha)" >&2
        rm -f "$tgz"
        exit 1
    fi
}

# The license of anything we link in has to travel with the binary.  Keep the
# copies inside the prefix, since that is what CI caches: on a cache hit the
# source trees below are never unpacked.
install_license() {  # $1 = source directory, $2 = name in the package
    mkdir -p "$PREFIX/share/licenses"
    for name in LICENSE.txt LICENSE COPYING.txt COPYING; do
        if [ -f "$1/$name" ]; then
            cp "$1/$name" "$PREFIX/share/licenses/$2.txt"
            return 0
        fi
    done
    echo "ERROR: no license file found in $1" >&2
    exit 1
}

if [ ! -f "$PREFIX/lib/libSDL3.a" ]; then
    echo "Building SDL3 $SDL3_VER (static) ..."
    fetch_tarball SDL3 "$SDL3_VER" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VER/SDL3-$SDL3_VER.tar.gz" \
        "$SDL3_TARBALL_SHA256"
    rm -rf "build/vendor/SDL3-$SDL3_VER"
    tar -xzf "build/vendor/SDL3-$SDL3_VER.tar.gz" -C build/vendor
    cmake -S "build/vendor/SDL3-$SDL3_VER" -B "build/vendor/SDL3-$SDL3_VER/build" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF >/dev/null
    cmake --build "build/vendor/SDL3-$SDL3_VER/build" -j"$NPROC" >/dev/null
    cmake --install "build/vendor/SDL3-$SDL3_VER/build" >/dev/null
    install_license "build/vendor/SDL3-$SDL3_VER" SDL3
fi

if [ ! -f "$PREFIX/lib/libSDL3_net.a" ]; then
    echo "Building SDL3_net $SDL3_NET_VER (static) ..."
    fetch_tarball SDL3_net "$SDL3_NET_VER" \
        "https://github.com/libsdl-org/SDL_net/releases/download/release-$SDL3_NET_VER/SDL3_net-$SDL3_NET_VER.tar.gz" \
        "$SDL3_NET_TARBALL_SHA256"
    rm -rf "build/vendor/SDL3_net-$SDL3_NET_VER"
    tar -xzf "build/vendor/SDL3_net-$SDL3_NET_VER.tar.gz" -C build/vendor
    cmake -S "build/vendor/SDL3_net-$SDL3_NET_VER" -B "build/vendor/SDL3_net-$SDL3_NET_VER/build" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DSDLNET_SAMPLES=OFF >/dev/null
    cmake --build "build/vendor/SDL3_net-$SDL3_NET_VER/build" -j"$NPROC" >/dev/null
    cmake --install "build/vendor/SDL3_net-$SDL3_NET_VER/build" >/dev/null
    install_license "build/vendor/SDL3_net-$SDL3_NET_VER" SDL3_net
fi

if [ "$DEPS_ONLY" = true ]; then
    echo "built: $PREFIX (static SDL3 $SDL3_VER + SDL3_net $SDL3_NET_VER)"
    exit 0
fi

if ! have_data "$DATA_ARG" && [ "$#" -ge 1 ]; then
    echo "ERROR: no Tyrian data in '$DATA_ARG'" >&2
    echo "Point make_linux.sh at your Tyrian 2.1 data, or run it with no argument" >&2
    echo "to download the freeware release into ./data automatically." >&2
    exit 1
fi
./get_data.sh "$DATA_ARG"
DATA_DIR="$(cd "$DATA_ARG" && pwd)"

# ---- the game, against the static SDL ----
# pkg-config resolves sdl3/sdl3-net from our prefix; --static pulls in the
# libraries SDL itself needs (m, dl, pthread, rt) for the static link.
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
make clean >/dev/null
make -j"$NPROC" \
    SDL_LDLIBS="$(pkg-config --static --libs-only-l sdl3 sdl3-net)"

echo
if ldd opentyrian | grep -q "libSDL3"; then
    echo "ERROR: opentyrian still links SDL3 dynamically" >&2
    exit 1
fi
echo "built: ./opentyrian (SDL3 linked statically)"
echo "run:   ./opentyrian --data=\"$DATA_DIR\""
