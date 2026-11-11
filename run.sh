#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
exec ./build/luma "$@"
