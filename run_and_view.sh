#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <output-image>"
    exit 1
fi

IMAGE="$1"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# cmake -S . -B build

cmake --build build --target physics_vfx --parallel

./build/physics_vfx -image "$IMAGE"

./build/DPA_ImageProcessing/dpa_imgviewer \
    -image "$IMAGE"