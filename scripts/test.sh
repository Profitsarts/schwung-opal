#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Host test: builds Opal Synth for this machine and drives it as Schwung's chain does; writes build/opal_analog.wav.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=gnu11 -O2 -w -Isrc -Isrc/dsp -Ivendor/felucca/gen -Ivendor/felucca/src \
    tests/test_synth.c src/dsp/opal_synth.c src/dsp/opal_core0.c src/dsp/opal_core1.c src/dsp/opal_core2.c \
    src/dsp/opal_core3.c -o build/test_synth -lm
build/test_synth build/opal_analog.wav
# the drawn pages (src/canvas.js), from the engines' ranges the test above wrote; writes build/opal_screens.png
if command -v node >/dev/null; then
    node tests/test_canvas.mjs build/viz.txt build/opal_screens.png
else
    echo "node not found: skipping the canvas test"
fi
