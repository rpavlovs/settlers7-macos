#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd -- "$(dirname -- "$0")/.."
if [ "$#" -ne 1 ]; then
    echo 'Usage: sh tests/run.sh EXISTING_CROSSOVER_BOTTLE' >&2
    exit 2
fi
bottle=$1
wine=${CROSSOVER_WINE:-/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine}
sh build.sh
python3 tests/test_launch.py
mkdir -p build/fixtures
for spec in '22050 1' '44100 2' '48000 2'; do
    rate=${spec% *}
    channels=${spec#* }
    fixture="build/fixtures/sine-$rate-$channels.mp3"
    ffmpeg -hide_banner -loglevel error -nostdin -y -f lavfi \
      -i "sine=frequency=997:sample_rate=$rate:duration=2" -ac "$channels" \
      -c:a libmp3lame -b:a 128k -write_xing 0 -map_metadata -1 "$fixture"
    "$wine" --bottle "$bottle" --debugmsg -all \
      "$PWD/build/tests/asi_checks.exe" "Z:$PWD/$fixture"
done
for mode in disabled enabled permanent opt-out; do
    "$wine" --bottle "$bottle" --debugmsg -all \
      "$PWD/build/tests/dep_checks.exe" "Z:$PWD/build/mssmp3.asi" "$mode"
done
printf '%s\n' 'All standalone checks passed. Real Miles/game integration is a separate check.'
