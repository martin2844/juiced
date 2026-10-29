#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then
    echo 'Usage: build.sh GAME_DATA WORK_DIRECTORY' >&2
    exit 2
fi
car_tools=$(cd -- "$(dirname -- "$0")" && pwd)
car_work=$(mkdir -p -- "$2" && cd -- "$2" && pwd)
node "$car_tools/decode.cjs" "$car_tools/assets/honda-integra-db8.glb" "$car_work/integra-decoded.glb"
node "$car_tools/decode.cjs" "$car_tools/assets/bmw-e36-m3.glb" "$car_work/bmw-decoded.glb"
node "$car_tools/decode.cjs" "$car_tools/assets/toyota-corolla-levin.glb" "$car_work/levin-decoded.glb"
blender -b --python "$car_tools/inspect_blender.py" -- "$car_work"
blender -b --python "$car_tools/prepare_blender.py" -- "$car_work"
python3 "$car_tools/import_cars.py" --game "$1" --sources "$car_work" --output "$car_work/overlay"
python3 "$car_tools/package.py" "$car_work/overlay" "$1"
