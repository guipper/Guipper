#!/usr/bin/env bash
set -euo pipefail

LIBFREENECT2_REPOSITORY="https://github.com/OpenKinect/libfreenect2.git"
LIBFREENECT2_REVISION="fd64c5d9b214df6f6a55b4419357e51083f15d93"
SOURCE_DIR="${TMPDIR:-/tmp}/guipper-libfreenect2"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This installer supports Linux only." >&2
  exit 1
fi

sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libusb-1.0-0-dev libturbojpeg0-dev libglfw3-dev

rm -rf "$SOURCE_DIR"
git clone "$LIBFREENECT2_REPOSITORY" "$SOURCE_DIR"
git -C "$SOURCE_DIR" checkout "$LIBFREENECT2_REVISION"
cmake -S "$SOURCE_DIR" -B "$SOURCE_DIR/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DENABLE_CXX11=ON \
  -DENABLE_CUDA=OFF \
  -DENABLE_OPENCL=OFF
cmake --build "$SOURCE_DIR/build" --parallel "$(nproc)"
sudo cmake --install "$SOURCE_DIR/build"
sudo cp "$SOURCE_DIR/platform/linux/udev/90-kinect2.rules" \
  /etc/udev/rules.d/90-kinect2.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo ldconfig

echo
echo "libfreenect2 $(pkg-config --modversion freenect2) installed."
echo "Reconnect the powered Kinect v2 directly to USB 3, then verify with:"
echo "  $SOURCE_DIR/build/bin/Protonect cpu"
echo "Rebuild Guipper afterward so config.make enables KINECT2."
