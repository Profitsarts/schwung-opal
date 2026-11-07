#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Installs the built module on a Move over SSH (Schwung's key setup).
#   ./scripts/build.sh && ./scripts/install.sh [host]      host defaults to move.local
set -euo pipefail
cd "$(dirname "$0")/.."
HOST="${1:-move.local}"
DEST=/data/UserData/schwung/modules/sound_generators/opal-synth
[ -d dist/opal-synth ] || { echo "dist/opal-synth missing: run ./scripts/build.sh first"; exit 1; }
echo "==> module -> $HOST:$DEST/"
ssh "ableton@$HOST" "mkdir -p $DEST"
scp -q dist/opal-synth/* "ableton@$HOST:$DEST/"
ssh "ableton@$HOST" "chmod -R a+rw $DEST"
echo "==> done. In the Shadow UI, load Opal Synth as a track's synth."
