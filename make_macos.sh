#!/bin/bash
# make_macos.sh — build a self-contained, universal OpenTyrian.app (macOS).
# Bundles the official SDL3.framework and SDL3_net.framework and the freeware
# Tyrian 2.1 data, so the app runs on any Mac with nothing installed.
#
# Usage: ./make_macos.sh [data-dir]     (default: ./data, fetched if missing)
set -euo pipefail

cd "$(dirname "$0")"
DATA_ARG="${1:-data}"

have_data() { find "$1" -maxdepth 1 -iname "tyrian1.lvl" 2>/dev/null | grep -q .; }

if ! have_data "$DATA_ARG" && [ "$#" -ge 1 ]; then
    echo "ERROR: no Tyrian data in '$DATA_ARG'" >&2
    echo "Point make_macos.sh at your Tyrian 2.1 data, or run it with no argument" >&2
    echo "to download the freeware release into ./data automatically." >&2
    exit 1
fi
./get_data.sh "$DATA_ARG"
DATA_DIR="$(cd "$DATA_ARG" && pwd)"

OUT="build/OpenTyrian.app"

# ---- official universal SDL3 / SDL3_net frameworks (arm64 + x86_64), cached ----
# The official macOS dmgs ship an .xcframework whose macos-arm64_x86_64 slice is
# the plain, universal .framework we bundle (headers, Resources/Versions layout
# and all).  Versions and their dmg SHA-256 are pinned; the release assets are
# immutable, and the checksum is verified before anything is extracted.
SDL3_VER="3.4.16"
SDL3_NET_VER="3.2.0"
SDL3_DMG_SHA256="675660a9e457239af615f9e41f788612168d1639b9d2eda2957e8dace26687fd"
SDL3_NET_DMG_SHA256="0f28e62ce224f2edd8badfc6d607854ec7d34650f56dfbf7f16377cb0f1086a5"
FW_DIR="$PWD/build/vendor"

# fetch_framework NAME VERSION URL SHA256
# Downloads the official dmg, verifies its checksum and extracts the universal
# macOS framework into build/vendor.  Cached: a second run does no network I/O.
fetch_framework() {
    local name="$1" ver="$2" url="$3" sha="$4"
    local fw="$FW_DIR/$name.framework"
    if [ -d "$fw" ]; then
        return 0
    fi
    echo "Downloading $name $ver framework (universal, cached) ..."
    mkdir -p "$FW_DIR"
    local dmg="$FW_DIR/$name-$ver.dmg"
    curl -fL --progress-bar -o "$dmg" "$url"
    if ! echo "$sha  $dmg" | shasum -a 256 -c - >/dev/null; then
        echo "ERROR: checksum mismatch for $name $ver (expected $sha)" >&2
        rm -f "$dmg"
        exit 1
    fi
    local mnt
    mnt=$(mktemp -d)
    hdiutil attach "$dmg" -nobrowse -quiet -mountpoint "$mnt"
    cp -R "$mnt/$name.xcframework/macos-arm64_x86_64/$name.framework" "$FW_DIR/"
    hdiutil detach "$mnt" -quiet
    rm -f "$dmg"
}

fetch_framework SDL3 "$SDL3_VER" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VER/SDL3-$SDL3_VER.dmg" \
    "$SDL3_DMG_SHA256"
fetch_framework SDL3_net "$SDL3_NET_VER" \
    "https://github.com/libsdl-org/SDL_net/releases/download/release-$SDL3_NET_VER/SDL3_net-$SDL3_NET_VER.dmg" \
    "$SDL3_NET_DMG_SHA256"
FW="$FW_DIR/SDL3.framework"
FW_NET="$FW_DIR/SDL3_net.framework"

# ---- one build per architecture, then lipo them together ----
# The Makefile's SDL_* variables normally come from pkg-config; overriding
# them on the command line points the build at the frameworks instead.  `-F`
# makes `#include <SDL3/SDL.h>` / `#include <SDL3_net/SDL_net.h>` resolve to the
# frameworks' flat Headers and lets the linker find them; the rpath matches
# where the bundle keeps them.
build_arch() {  # $1 = arm64 | x86_64
    make clean >/dev/null
    make -j"$(sysctl -n hw.ncpu)" \
        CC="cc -arch $1 -mmacosx-version-min=10.13" \
        WITH_NETWORK=true \
        SDL_CPPFLAGS="-F$FW_DIR" \
        SDL_LDFLAGS="-F$FW_DIR -Wl,-rpath,@executable_path/../Frameworks" \
        SDL_LDLIBS="-framework SDL3_net -framework SDL3" >/dev/null
    mv opentyrian "build/opentyrian-$1"
}
mkdir -p build
build_arch arm64
build_arch x86_64
lipo -create -output build/opentyrian-universal build/opentyrian-arm64 build/opentyrian-x86_64

# ---- assemble the bundle ----
rm -rf "$OUT"
mkdir -p "$OUT/Contents/MacOS" "$OUT/Contents/Resources" "$OUT/Contents/Frameworks"

VERSION="$( (git describe --tags || git rev-parse --short HEAD) 2>/dev/null || echo dev)"
cat > "$OUT/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>      <string>opentyrian</string>
    <key>CFBundleIdentifier</key>      <string>io.github.opentyrian.OpenTyrian</string>
    <key>CFBundleName</key>            <string>OpenTyrian</string>
    <key>CFBundleDisplayName</key>     <string>OpenTyrian</string>
    <key>CFBundlePackageType</key>     <string>APPL</string>
    <key>CFBundleShortVersionString</key> <string>$VERSION</string>
    <key>CFBundleIconFile</key>        <string>OpenTyrian</string>
    <key>LSMinimumSystemVersion</key>  <string>10.13</string>
    <key>NSHighResolutionCapable</key> <true/>
</dict>
</plist>
PLIST

cp build/opentyrian-universal "$OUT/Contents/MacOS/opentyrian"

# SDL3.framework + SDL3_net.framework, headers stripped
cp -R "$FW" "$OUT/Contents/Frameworks/"
cp -R "$FW_NET" "$OUT/Contents/Frameworks/"
rm -rf "$OUT/Contents/Frameworks/SDL3.framework/Headers" \
       "$OUT/Contents/Frameworks/SDL3.framework/Versions/A/Headers" \
       "$OUT/Contents/Frameworks/SDL3_net.framework/Headers" \
       "$OUT/Contents/Frameworks/SDL3_net.framework/Versions/A/Headers"

# Game data: the engine looks in <bundle>/Contents/Resources/data (lowercase names)
mkdir -p "$OUT/Contents/Resources/data"
find "$DATA_DIR" -maxdepth 1 -type f | while read -r f; do
    cp "$f" "$OUT/Contents/Resources/data/$(basename "$f" | tr '[:upper:]' '[:lower:]')"
done

# Licenses.  SDL3's and SDL3_net's travel inside their frameworks already, but
# three levels down where nobody would look for them.
cp COPYING "$OUT/Contents/Resources/COPYING.txt"
mkdir -p "$OUT/Contents/Resources/licenses"
cp "$FW/Versions/A/Resources/LICENSE.txt" "$OUT/Contents/Resources/licenses/SDL3.txt"
cp "$FW_NET/Versions/A/Resources/LICENSE.txt" "$OUT/Contents/Resources/licenses/SDL3_net.txt"
cp doc/tyrian-freeware-license.txt "$OUT/Contents/Resources/licenses/Tyrian.txt"

# App icon from the 128px Linux icon
python3 make_icon.py linux/icons/tyrian-128.png "$OUT/Contents/Resources/OpenTyrian.icns" \
    && echo "icon: from linux/icons/tyrian-128.png" || echo "note: icon generation failed, skipping"

codesign --force -s - "$OUT/Contents/Frameworks/SDL3_net.framework"
codesign --force -s - "$OUT/Contents/Frameworks/SDL3.framework"
codesign --force -s - "$OUT"
touch "$OUT"
echo "built: $OUT"
