#!/usr/bin/env bash
set -euo pipefail
trap 'echo "No se pudo instalar Seguimiento. Revisá la conexión, Python/venv y el error anterior." >&2' ERR
[[ "$(uname -s)" == Linux ]] || { echo 'Disponible solo en Linux.' >&2; exit 1; }
runtime_dir="${GUIPPER_HAND_RUNTIME:-${XDG_DATA_HOME:-$HOME/.local/share}/Guipper/hand-tracking}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3,12) else "Se necesita Python 3.12 o posterior para este runtime fijado.")'
mkdir -p "$runtime_dir"
python3 -m venv --without-pip "$runtime_dir/venv"
python3 -m pip --python "$runtime_dir/venv/bin/python" install --index-url https://pypi.org/simple --disable-pip-version-check 'pip==25.3'
"$runtime_dir/venv/bin/python" -m pip install --index-url https://pypi.org/simple --disable-pip-version-check -r "$script_dir/hand_tracking/requirements.lock"
curl --fail --location --retry 3 --output "$runtime_dir/hand_landmarker.task.download" \
  https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task
printf '%s  %s\n' fbc2a30080c3c557093b5ddfc334698132eb341044ccee322ccf8bcf3607cde1 "$runtime_dir/hand_landmarker.task.download" | sha256sum --check
mv "$runtime_dir/hand_landmarker.task.download" "$runtime_dir/hand_landmarker.task"
curl --fail --location --retry 3 --output "$runtime_dir/pose_landmarker_lite.task.download" \
  https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task
printf '%s  %s\n' 59929e1d1ee95287735ddd833b19cf4ac46d29bc7afddbbf6753c459690d574a "$runtime_dir/pose_landmarker_lite.task.download" | sha256sum --check
mv "$runtime_dir/pose_landmarker_lite.task.download" "$runtime_dir/pose_landmarker_lite.task"
cp "$script_dir/hand_tracking/worker.py" "$runtime_dir/worker.py"
"$runtime_dir/venv/bin/python" -m pip freeze > "$runtime_dir/installed-versions.txt"
echo "Seguimiento instalado en $runtime_dir. En Guipper: SETTINGS > Seguimiento > Manos o Cuerpo > Iniciar."
