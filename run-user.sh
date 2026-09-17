#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
unset GUIPPER_CURATED_LIST
export GUIPPER_USER_LIST="${GUIPPER_USER_LIST:-$project_dir/release/shader-curated.json}"
export GUIPPER_USER_ROOT="${GUIPPER_USER_ROOT:-$project_dir/dist/user-profile}"
if [[ ! -x "$project_dir/bin/Guipper" ]]; then
    echo 'Falta compilar Guipper: ejecutá make -j2 desde el proyecto.' >&2
    exit 1
fi
cd "$project_dir/bin"
exec ./Guipper "$@"
