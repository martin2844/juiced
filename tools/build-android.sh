#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project"
export ANDROID_HOME="${ANDROID_HOME:-$project/out/android-sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
signing="${OJ_SIGNING_ENV:-$HOME/.local/share/openjuice/android-signing.env}"
variant="${1:-release}"
abi="${2:-arm64-v8a}"
if [[ -f "$signing" ]]; then
    set -a
    source "$signing"
    set +a
fi
if [[ "$variant" == release && -z "${OJ_KEYSTORE:-}" ]]; then
    echo "Release signing requires OJ_KEYSTORE, OJ_STORE_PASSWORD and OJ_KEY_PASSWORD, or $signing" >&2
    exit 1
fi
case "$variant" in
  release) task=assembleRelease ;;
  debug) task=assembleDebug ;;
  test) task=assembleDebugAndroidTest ;;
  *) echo "Usage: $0 [release|debug|test] [arm64-v8a|x86_64|arm64-v8a,x86_64]" >&2; exit 1 ;;
esac
cd android
./gradlew --no-daemon --max-workers=4 "-Pabi=$abi" "$task"
