#!/usr/bin/env bash
# Builds dist/lms: a folder with lms.exe and every file it needs, so it runs by
# double-click on a computer that does not have MSYS2 or Qt installed.
set -e
cd "$(dirname "$0")/.."
OUT=dist/lms

cmake -S . -B build -G Ninja -DLMS_BUILD_UI=ON > /dev/null
cmake --build build --target lms

rm -rf "$OUT"
mkdir -p "$OUT/platforms" "$OUT/styles"
cp build/src/ui/lms.exe "$OUT/"

# Copy every DLL that a file needs from the MSYS2 folder (ldd lists them all).
copy_deps() {
  ldd "$1" | awk '/ucrt64/ {print $3}' | while read -r dll; do cp -n "$dll" "$OUT/"; done
}
copy_deps "$OUT/lms.exe"

# The Qt plugins: "windows" lets the program draw windows, "styles" gives the normal look.
PLUGINS=$(ls -d /ucrt64/share/qt6/plugins /ucrt64/lib/qt6/plugins 2>/dev/null | head -1)
cp "$PLUGINS/platforms/qwindows.dll" "$OUT/platforms/"
copy_deps "$OUT/platforms/qwindows.dll"
if [ -f "$PLUGINS/styles/qmodernwindowsstyle.dll" ]; then
  cp "$PLUGINS/styles/qmodernwindowsstyle.dll" "$OUT/styles/"
  copy_deps "$OUT/styles/qmodernwindowsstyle.dll"
fi

echo "Package ready: $OUT  ($(ls "$OUT" | wc -l) items, $(du -sh "$OUT" | cut -f1))"
