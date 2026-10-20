# Reproducing the checks

Run `sh tests/run.sh 'Existing bottle name'` from the repository root with
CrossOver, MinGW-w64, Python 3 and FFmpeg (`libmp3lame`) available.
Set `CROSSOVER_WINE` if CrossOver is installed elsewhere. The test uses the named
existing bottle but does not restart it or launch the game. All generated data
and binaries stay in ignored `build/`. The default macOS `Z:` drive mapping is
used to address these files from Wine.

`asi_checks.c` exercises the actual adapter implementation with synthetic MP3s.
`dep_checks.c` loads the built plugin and tests policy transitions only inside
its own disposable Wine process. `test_launch.py` runs the real shell launcher
against a stub executable, so it can verify argument/environment handling
without starting CrossOver. These do not replace an actual game test.

## Optional concurrent Miles benchmark

This needs private copies of your own game's `mss32.dll` and `mssdsp.flt`.
Create two directories under `build/`, each containing those DLLs and exactly
one `mssmp3.asi`: the original in one, this build in the other. Do not use the
game's live directory or include both codecs in one test directory. Never
upload these proprietary DLLs.

Generate a ten-second CBR sample:

```sh
ffmpeg -hide_banner -loglevel error -f lavfi \
  -i 'anoisesrc=color=pink:sample_rate=44100:duration=10:seed=7' \
  -ac 2 -c:a libmp3lame -b:a 128k -write_xing 0 -map_metadata -1 \
  build/fixtures/stream.mp3
```

Run `build/tests/stream_bench.exe` through CrossOver with three Windows-path
arguments: the private codec directory, the generated MP3, and `16`. For example,
substitute your own checkout and bottle paths:

```sh
"/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine" \
  --bottle 'Existing bottle name' --debugmsg -all \
  "$PWD/build/tests/stream_bench.exe" \
  "Z:$PWD/build/private-original" "Z:$PWD/build/fixtures/stream.mp3" 16
```

Repeat using the directory containing the replacement. The benchmark mutes
its own digital driver, reports stream progress and query latency, and has a
30-second watchdog. An original-codec timeout or incomplete-stream exit is a
possible reproduction result, not a reason to change the game's audio files.

## Optional graphics-memory reproduction

Copy Sarek's 32-bit `d3d9.dll` beside `build/tests/graphics_bench.exe`. Use the
regular MoltenVK 1.4.2 library and the same graphics environment as `launch.sh`:
`CX_LIBVULKAN` must be in CrossOver's Unix environment, not just set in a Windows
batch file. Set `WINEDLLOVERRIDES=d3d9=n,b`,
`DXVK_CONFIG=d3d9.splitSamplerSlots=True` and
`MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1` through CrossOver's `--env` option.

Run `graphics_bench.exe` with these alternatives:

- No arguments: baseline with the executable's normal DEP policy.
- `nt-dep-off`: deliberately disable DEP for this disposable test process only.
- `nt-dep-off Z:\path\to\build\mssmp3.asi`: disable DEP, then invoke the plugin's
  initializer to apply the correction before graphics allocation.

Compare the first managed-texture `lock_ms`. This benchmark creates small
hidden D3D9 resources and performs no presentation or game simulation. It
cannot measure game FPS. It has a 30-second watchdog and a short library-check
pause. The DEP toggle is for this reproduction only; it is not an installation
instruction or a system-wide change.
