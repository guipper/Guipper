#!/usr/bin/env bash

set -u

launcher_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
guipper_binary="${launcher_dir}/Guipper"
gpu_mode="${GUIPPER_GPU:-auto}"

if [[ ! -x "${guipper_binary}" ]]; then
    echo "Guipper executable not found: ${guipper_binary}" >&2
    exit 1
fi

cd -- "${launcher_dir}"

case "${gpu_mode}" in
    auto)
        if command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi -L >/dev/null 2>&1; then
            echo "Guipper: using NVIDIA PRIME render offload"
            exec env __NV_PRIME_RENDER_OFFLOAD=1 \
                __GLX_VENDOR_LIBRARY_NAME=nvidia \
                "${guipper_binary}" "$@"
        fi
        echo "Guipper: NVIDIA PRIME unavailable; using the system default GPU"
        exec "${guipper_binary}" "$@"
        ;;
    nvidia)
        echo "Guipper: forcing NVIDIA PRIME render offload"
        exec env __NV_PRIME_RENDER_OFFLOAD=1 \
            __GLX_VENDOR_LIBRARY_NAME=nvidia \
            "${guipper_binary}" "$@"
        ;;
    default)
        echo "Guipper: using the system default GPU"
        exec "${guipper_binary}" "$@"
        ;;
    *)
        echo "Unknown GUIPPER_GPU mode '${gpu_mode}' (use auto, nvidia, or default)" >&2
        exit 2
        ;;
esac
