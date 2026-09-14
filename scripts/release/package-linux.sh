#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"
: "${LINUXDEPLOY:?Set LINUXDEPLOY to a reviewed linuxdeploy executable}"
: "${APPIMAGETOOL:?Set APPIMAGETOOL to a reviewed appimagetool executable}"
: "${APPIMAGE_RUNTIME:?Set APPIMAGE_RUNTIME to the pinned runtime-x86_64}"
python3 scripts/release/verify_tools.py "$(dirname -- "$LINUXDEPLOY")"
for tool in "$APPIMAGETOOL" "$APPIMAGE_RUNTIME"; do
    test "$(dirname -- "$tool")" = "$(dirname -- "$LINUXDEPLOY")" || { echo "Use the verified tool directory for all packaging tools" >&2; exit 1; }
done
version="$(cat VERSION)"
appdir="$project_root/dist/Guipper-$version.AppDir"
python3 scripts/release/stage.py --output "$appdir/usr/bin" --binary "$project_root/bin/Guipper"
cp release/Guipper.desktop "$appdir/Guipper.desktop"
python3 scripts/release/icon.py bin/data/guipper.png "$appdir/guipper.svg"
"$LINUXDEPLOY" --appdir "$appdir" --executable "$appdir/usr/bin/Guipper" --desktop-file "$appdir/Guipper.desktop" --icon-file "$appdir/guipper.svg"
# linuxdeploy may create AppRun as a symlink to the application binary.
# Replace the link itself before writing our launcher.
rm -f -- "$appdir/AppRun"
cp bin/launch-guipper.sh "$appdir/usr/bin/launch-guipper.sh"
chmod +x "$appdir/usr/bin/launch-guipper.sh"
cat > "$appdir/AppRun" <<'RUN'
#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$root/usr/bin"
export LD_LIBRARY_PATH="$root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./launch-guipper.sh "$@"
RUN
chmod +x "$appdir/AppRun"
# Keep the previous candidate usable if packaging fails or it is running.
output="$project_root/dist/Guipper-$version-linux-x64.AppImage"
pending="$output.tmp-$$.AppImage"
trap 'rm -f -- "$pending"' EXIT
ARCH=x86_64 "$APPIMAGETOOL" --runtime-file "$APPIMAGE_RUNTIME" "$appdir" "$pending"
mv -f -- "$pending" "$output"
