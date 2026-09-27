#!/usr/bin/env bash
# Regenerates the images in docs/screens/ for OPERATION.md from the real UI
# code: renders the OLED screens on the PC, then styles them and draws the
# screen map and timing diagram. Needs gcc, python3, Pillow, matplotlib.
set -euo pipefail
cd "$(dirname "$0")/.."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -Isrc -Itest/fake_sdk -o "$TMP/render" docs/render_screens.c \
    test/fake_sdk/sim.c test/fake_board.c test/fake_crank.c test/fake_dac.c \
    src/ui_core.c src/gfx.c src/ecu.c src/knock.c src/knock_codec.c src/crank_timing.c \
    src/dac_codec.c -lm
"$TMP/render" "$TMP" >/dev/null
python3 docs/make_images.py "$TMP" docs/screens
