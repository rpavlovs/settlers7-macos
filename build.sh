#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd -- "$(dirname -- "$0")"
cc=${CC:-i686-w64-mingw32-gcc}
mkdir -p build/MacCompatibility build/tests
"$cc" -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows \
  src/launcher.c -o build/MacCompatibility/Settlers7-Mac.exe -lshell32 -luser32
"$cc" -std=c11 -O3 -Wall -Wextra -Werror -msse2 -mfpmath=sse \
  -mstackrealign -fno-strict-aliasing -static-libgcc -shared \
  src/asi_minimp3.c src/dep_compat.c src/asi_minimp3.def \
  -Ivendor/minimp3 -o build/mssmp3.asi
"$cc" -std=c11 -O3 -Wall -Wextra -Werror -msse2 -mfpmath=sse \
  -mstackrealign -fno-strict-aliasing -static-libgcc \
  tests/asi_checks.c src/dep_compat.c -Ivendor/minimp3 -o build/tests/asi_checks.exe
"$cc" -std=c11 -O2 -Wall -Wextra -Werror \
  tests/dep_checks.c -o build/tests/dep_checks.exe
"$cc" -std=c11 -O2 -Wall -Wextra -Werror \
  tests/stream_bench.c -o build/tests/stream_bench.exe
"$cc" -std=c11 -O2 -Wall -Wextra -Werror \
  tests/graphics_bench.c -o build/tests/graphics_bench.exe -luser32
cp launch.sh build/MacCompatibility/launch.sh
printf '%s\n' 'Built compatibility files and standalone tests in build/.'
