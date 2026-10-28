#!/usr/bin/env bash
set -euo pipefail

prefix=${1:?Usage: gamma_sequence.sh path/to/sphere_pyro [gamma]}
gamma=${2:-2.2}

shopt -s nullglob
files=( "${prefix}".[0-9][0-9][0-9][0-9].exr )

if ((${#files[@]} == 0)); then
    printf 'No matching frames found for: %s.####.exr\n' "$prefix" >&2
    exit 1
fi

for input in "${files[@]}"; do
    frame=${input%.*}
    frame=${frame##*.}
    output="${prefix}_gamma_corrected.${frame}.exr"

    oiiotool "$input" --powc "$gamma,$gamma,$gamma,1" -o "$output"
done

ffmpeg -framerate 24 \
    -i "${prefix}_gamma_corrected.%04d.exr" \
    -vf "vflip,format=yuv420p" \
    -c:v mpeg4 -q:v 2 \
    "${prefix}_gamma_corrected.mov"