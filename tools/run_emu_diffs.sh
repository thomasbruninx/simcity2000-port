#!/bin/sh
# Run every differential test (port vs. original machine code in the Unicorn emulator) on the sample cities.
# Usage: tools/run_emu_diffs.sh <SC2K dir> [build dir]
set -e
D="${1:?usage: run_emu_diffs.sh <SC2K dir> [build dir]}"; B="${2:-build}"
for mode in power water env demand finance; do
  "$B/sc2k-emu-diff" "$mode" "$D/SIMCITY.EXE" "$D"/CITIES/*.SC2 "$D"/SCENARIO/*.SCN "$D/DEFAULT.SC2" | tail -1
done
