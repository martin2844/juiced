#!/bin/sh
# Usage: build.sh /path/to/retail/Juiced /path/to/output-mod
set -eu
if [ "$#" -ne 2 ]; then
    echo 'Usage: build.sh GAME_DATA OUTPUT_MOD' >&2
    exit 2
fi
car_tool_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
car_work_dir=$(mktemp -d "${TMPDIR:-/tmp}/juiced-car.XXXXXX")
trap 'rm -rf "$car_work_dir"' EXIT HUP INT TERM
python3 -m venv "$car_tool_dir/.venv"
"$car_tool_dir/.venv/bin/python" -m pip install -r "$car_tool_dir/requirements.txt"
npm ci --prefix "$car_tool_dir" --no-audit --no-fund
node "$car_tool_dir/decode.cjs" "$car_tool_dir/assets/ferrari.glb" "$car_work_dir/decoded.json"
"$car_tool_dir/.venv/bin/python" "$car_tool_dir/import_ferrari.py" --game "$1" --decoded "$car_work_dir/decoded.json" --output "$2"
