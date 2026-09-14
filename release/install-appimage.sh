#!/bin/sh
# Invoked with argv (never a shell command string), only after signature validation.
set -eu
original=$1
candidate=$2
parent=$3
health=$4
# Wait until Guipper has saved, closed outputs and exited.
while kill -0 "$parent" 2>/dev/null; do sleep 1; done
backup="${original}.previous"
if [ -e "$backup" ]; then
    archived="${backup}.$(date +%s)"
    if [ -e "$archived" ]; then exit 1; fi
    mv -- "$backup" "$archived"
fi
mv -- "$original" "$backup"
if ! mv -- "$candidate" "$original"; then mv -- "$backup" "$original"; exit 1; fi
GUIPPER_UPDATE_HEALTH_FILE="$health" "$original" &
child=$!
count=0
while [ "$count" -lt 45 ]; do
    if [ -f "$health" ]; then exit 0; fi
    if ! kill -0 "$child" 2>/dev/null; then break; fi
    sleep 1
    count=$((count+1))
done
# Never kill a running renderer to force a rollback. A hung first launch is
# reported for manual recovery; a failed process is safe to roll back.
if kill -0 "$child" 2>/dev/null; then exit 1; fi
mv -- "$original" "${original}.failed"
mv -- "$backup" "$original"
"$original" &
