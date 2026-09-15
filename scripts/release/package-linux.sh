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
signed=false
if test -f src/JPutils/jp_update_config.h && grep -q '^#define GUIPPER_APPIMAGE_UPDATES 1' src/JPutils/jp_update_config.h; then
    signed=true
    : "${GUIPPER_UPDATE_SDK:?Build the pinned AppImageUpdate SDK first}"
    : "${GUIPPER_UPDATE_INFORMATION:?Set this package stable/beta update information}"
    : "${GUIPPER_SIGNING_KEY:?Set the full OpenPGP signing fingerprint}"
    : "${GUIPPER_DOWNLOAD_URL:?Set the final immutable HTTPS package URL}"
    python3 - "$GUIPPER_SIGNING_KEY" "$GUIPPER_UPDATE_INFORMATION" <<'CHECK'
import json, re, sys
from pathlib import Path
text=Path('src/JPutils/jp_update_config.h').read_text()
values={name:json.loads(value) for name,value in re.findall(r'^#define (GUIPPER_APPIMAGE_\w+) (".*")$',text,re.M)}
if values.get('GUIPPER_APPIMAGE_SIGNING_FINGERPRINT')!=sys.argv[1].upper():
    raise SystemExit('Build/signing key mismatch')
if sys.argv[2] not in [values.get('GUIPPER_APPIMAGE_STABLE'),values.get('GUIPPER_APPIMAGE_BETA')]:
    raise SystemExit('Package/build channel mismatch')
CHECK
    test -x "$GUIPPER_UPDATE_SDK/bin/guipper-update-worker"
    python3 - <<'BINARY'
from pathlib import Path
import re, json
text=Path('src/JPutils/jp_update_config.h').read_text()
values={name:json.loads(value) for name,value in re.findall(r'^#define (GUIPPER_APPIMAGE_\w+) (".*")$',text,re.M)}
expected='GUIPPER_LINUX_UPDATES_V1\n'+'\n'.join(values['GUIPPER_APPIMAGE_'+name] for name in ['STABLE','BETA','SIGNING_FINGERPRINT'])
if expected.encode() not in Path('bin/Guipper').read_bytes():
    raise SystemExit('Rebuild Guipper after generating update configuration')
BINARY
fi
package_root="${GUIPPER_PACKAGE_DIR:-$project_root/dist}"
mkdir -p -- "$package_root"
appdir="$package_root/Guipper-$version.AppDir"
python3 scripts/release/stage.py --output "$appdir/usr/bin" --binary "$project_root/bin/Guipper"
if $signed; then
    cp "$GUIPPER_UPDATE_SDK/bin/guipper-update-worker" "$appdir/usr/bin/"
    cp -r "$GUIPPER_UPDATE_SDK/licenses" "$appdir/usr/bin/update-licenses"
    cp "$GUIPPER_UPDATE_SDK/UPDATE-SOURCE.tar.gz" "$appdir/usr/bin/"
    cp "$GUIPPER_UPDATE_SDK/sources.json" "$appdir/usr/bin/UPDATE-SOURCES.json"
fi
cp release/Guipper.desktop "$appdir/Guipper.desktop"
python3 scripts/release/icon.py bin/data/guipper.png "$appdir/guipper.svg"
extra=()
if $signed; then extra+=(--executable "$appdir/usr/bin/guipper-update-worker"); fi
LD_LIBRARY_PATH="${GUIPPER_UPDATE_SDK:+$GUIPPER_UPDATE_SDK/lib:}${LD_LIBRARY_PATH:-}" "$LINUXDEPLOY" "${extra[@]}" --appdir "$appdir" --executable "$appdir/usr/bin/Guipper" --desktop-file "$appdir/Guipper.desktop" --icon-file "$appdir/guipper.svg"
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
output="$package_root/Guipper-$version-linux-x64.AppImage"
if $signed; then
    python3 scripts/release/sign-linux.py --appdir "$appdir" --output "$output" \
        --sdk "$GUIPPER_UPDATE_SDK" --tool "$APPIMAGETOOL" --runtime "$APPIMAGE_RUNTIME" \
        --info "$GUIPPER_UPDATE_INFORMATION" --key "$GUIPPER_SIGNING_KEY" --url "$GUIPPER_DOWNLOAD_URL"
    exit 0
fi
pending="$output.tmp-$$.AppImage"
trap 'rm -f -- "$pending"' EXIT
ARCH=x86_64 "$APPIMAGETOOL" --runtime-file "$APPIMAGE_RUNTIME" "$appdir" "$pending"
mv -f -- "$pending" "$output"
