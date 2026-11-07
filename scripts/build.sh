#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
#
# Cross-compiles Opal Synth for the Move (aarch64) and packages dist/.
# Uses a local aarch64-linux-gnu-gcc when there is one, the native cc on an aarch64 machine (CI runs on
# GitHub's arm64 runners), else the Docker image in scripts/Dockerfile.
# The tarball's top-level directory is the module id: Schwung extracts it into modules/sound_generators/.
# Felucca's sources are built as they are (-w: its own warnings are its own).
set -euo pipefail
cd "$(dirname "$0")/.."

ID=opal-synth
IMAGE=schwung-opal-builder
SRCS="src/dsp/opal_synth.c src/dsp/opal_core0.c src/dsp/opal_core1.c src/dsp/opal_core2.c src/dsp/opal_core3.c"
FLAGS="-std=gnu11 -O2 -w -shared -fPIC -Isrc -Isrc/dsp -Ivendor/felucca/gen -Ivendor/felucca/src"

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    echo "==> compiling with local aarch64-linux-gnu-gcc"
    # shellcheck disable=SC2086
    aarch64-linux-gnu-gcc $FLAGS $SRCS -o dsp.so -lm
elif [ "$(uname -m)" = aarch64 ] && command -v cc >/dev/null 2>&1; then
    echo "==> compiling with the native cc (aarch64 host)"
    # shellcheck disable=SC2086
    cc $FLAGS $SRCS -o dsp.so -lm
else
    docker build -q -t "$IMAGE" -f scripts/Dockerfile scripts/ >/dev/null
    echo "==> compiling with aarch64-linux-gnu-gcc (Docker)"
    docker run --rm -v "$PWD":/build -w /build "$IMAGE" \
        aarch64-linux-gnu-gcc $FLAGS $SRCS -o /build/dsp.so -lm
fi

echo "==> packaging dist/"
rm -rf dist && mkdir -p "dist/$ID"
mv dsp.so "dist/$ID/dsp.so"
cp src/module.json src/canvas.js LICENSE CREDITS.md "dist/$ID/"
( cd dist && tar -czf "$ID-module.tar.gz" "$ID/" )
echo "==> done: dist/$ID-module.tar.gz"
