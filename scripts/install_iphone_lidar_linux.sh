#!/usr/bin/env bash
set -euo pipefail

# iPhone LiDAR input through the Record3D app's USB streaming mode.
# Only distribution packages: usbmuxd talks to the phone, liblzfse decodes the
# depth frames. Nothing is built from source.

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This installer supports Linux only." >&2
  exit 1
fi

sudo apt-get update
# libusbmuxd-2.0.pc requires the glue and plist .pc files, which the -dev
# package does not pull in on its own.
sudo apt-get install -y \
  build-essential pkg-config \
  usbmuxd libimobiledevice-utils \
  libusbmuxd-dev libplist-dev libimobiledevice-glue-dev liblzfse-dev

sudo systemctl enable --now usbmuxd.service 2>/dev/null || true

echo
echo "usbmuxd $(pkg-config --modversion libusbmuxd-2.0) and liblzfse installed."
echo "Next:"
echo "  1. Connect the iPhone by cable, unlock it and accept 'Trust this computer'."
echo "     Verify with: idevice_id -l   (pairing: idevicepair pair)"
echo "  2. In Record3D: Settings -> enable 'USB Streaming mode', then press record."
echo "  3. Rebuild Guipper so config.make enables IPHONE_LIDAR:"
echo "       make Release JOBS=-j2 -j2"
