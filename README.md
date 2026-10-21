# Settlers 7 on macOS

Experimental compatibility code for **The Settlers 7: History Edition**, the
Windows Steam release, on Apple Silicon. Two tested paths are documented:
CrossOver and an experimental native ARM64 Wine/FEX engine. Neither uses a
virtual machine.

The CrossOver path combines an MP3 decoder adapter, a process-local Wine DEP
workaround, and a launcher for a tested DXVK-Sarek/MoltenVK configuration.
It requires your own installed game, its existing Miles sound library, Steam,
Ubisoft Connect, and CrossOver. It does not include game files or change
authentication, the original game executable, CrossOver licensing, or macOS security settings.

**Status:** an earlier local build passed basic gameplay checks in a small match on an M1 Max:
save/load, camera movement, road construction, material delivery and building
completion worked. Foreground gameplay HUD snapshots were mostly 20–30 fps,
with dips to 13–17 fps. The animated main menu remained around 5–8 fps.
These are snapshots, not a controlled FPS average. Long sessions, large maps
and other Macs are untested. See [validation and limits](docs/VALIDATION.md)
for the distinction between that gameplay test and this cleaned source build.

## Native ARM64 / FEX experiment

The [experimental ARM64/FEX source bundle](experimental/arm64-fex/README.md)
adds four pinned runtime patches, build notes, two focused reproductions and a
launch-environment template. In a small match on the same M1 Max, reduced x87
precision gave approximately 90–98 fps in stationary HUD observations. Saving,
reloading, construction, input and normal exit worked. One intermittent menu
crash remains unresolved; precision trade-offs and long-session limits apply.
This is a developer reproduction, not a ready-made installer. See the
[measured graphics settings](docs/GRAPHICS.md) for a tested quality preset near
60 fps at 2056x1285 in macOS fullscreen in the small match. The CrossOver
instructions below remain separate.

## What the code changes

- `mssmp3.asi` implements the Miles 7.2g ASI interface using
  [minimp3](https://github.com/lieff/minimp3), compiled for SSE2. It replaces the
  MP3 codec while retaining the game's original `mss32.dll` and mixer.
- On Wine only, the plugin enables **non-permanent, process-local DEP** when
  execution is unrestricted. That removed severe Rosetta overhead when writing
  GPU-visible memory in the tested setup. It preserves other policy flags and
  skips permanent policies. The underlying issue was already investigated by
  athei and fixed in
  [Highball's Wine engine](https://github.com/gauthierpiarrette/highball/releases/tag/v0.10.5).
  Highball is a separate project, not the upstream of CrossOver.
- `Settlers7-Mac.exe` and `launch.sh` apply graphics settings to this game's
  launch, inherit its CrossOver bottle, disable its Steam/Ubisoft overlays, and
  wait for the launched command. They do not change the bottle's global settings.

## Build

Use a 32-bit MinGW-w64 C compiler providing `i686-w64-mingw32-gcc` on your PATH.
On macOS, Homebrew provides it in the
[mingw-w64 formula](https://formulae.brew.sh/formula/mingw-w64).
For tests, also install Python 3 and FFmpeg with the `libmp3lame` encoder.

```sh
git clone https://github.com/rpavlovs/settlers7-macos.git
cd settlers7-macos
sh build.sh
```

Outputs are `build/mssmp3.asi`, `build/MacCompatibility/Settlers7-Mac.exe`,
`build/MacCompatibility/launch.sh` and standalone test programs.
The build does not install anything into your game or download dependencies.
Only the pinned minimp3 header is vendored. See [credits](CREDITS.md).

## Install into your existing game

The tested versions are CrossOver **26.3.0**, Settlers 7 History Edition
**1.12.1395.0**, DXVK-Sarek **1.14.1**, and MoltenVK **1.4.2**.
CrossOver's bottle graphics setting was **Auto**. Other combinations need testing.
First make sure Steam and Ubisoft Connect can authenticate and start your game.
Close the game before changing its files.

1. In Steam, open the game's **Manage → Browse local files**. Within that game
   directory, find `Data/Base/Bin/Settlers7R.exe` and `mssmp3.asi`.
2. Back up the original `mssmp3.asi`, any existing `d3d9.dll`, and the game's
   current Steam Launch Options **outside the game directory**. Keep those
   backups. This codec replaces an existing file.
3. Copy `build/mssmp3.asi` into `Data/Base/Bin/`, replacing the backed-up codec.
4. Download [DXVK-Sarek 1.14.1](https://github.com/pythonlover02/dxvk-sarek/releases/tag/v1.14.1).
   From its archive, copy **`build/x32/d3d9.dll`** into `Data/Base/Bin/`.
   Use the 32-bit DLL. Do not copy the other DXVK DLLs into the bottle.
5. Copy the built `MacCompatibility` directory into the game's `Data/` directory.
   Download [MoltenVK 1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2),
   choosing **`MoltenVK-macos.tar`**. Extract
   `MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib` and put it in
   `Data/MacCompatibility/`. Use this regular macOS build, not the private-API archive.
6. Set the game's Steam Launch Options to the following, adjusting the Windows
   game-library path if yours differs:

   ```text
   "C:\Program Files (x86)\Steam\steamapps\common\The Settlers 7 - History Edition\Data\MacCompatibility\Settlers7-Mac.exe" %command%
   ```

7. Start using **Play** in Windows Steam inside CrossOver. For the measured
   configuration, use a **1280×720 window**, Low object detail, shader quality,
   water, clutter and view range, High textures, and depth of field off. Object
   detail takes effect when a map loads. Back up your options before changing them.

The external files used in testing had these SHA-256 checksums:

```text
d3d9.dll (Sarek x32):
581f0a202aa131f8fa13fe884d342a45e17e1521019ff73b1aea48896a071832
libMoltenVK.dylib (regular macOS):
aef00b13bcc808adf15b85bef9ae67393d92be7ed5dfe41cad16fa809e4a4c5f
```

The launcher uses CrossOver's inherited `CX_ROOT` and `CX_BOTTLE`. For a custom
launch environment, `CROSSOVER_WINE` and `SETTLERS7_BOTTLE` override them. The
fallback CrossOver path is `/Applications/CrossOver.app`. A missing bottle,
library or command produces an error instead of selecting another bottle.
For normal Steam Play use, no override is needed. To override values while
starting through Steam, add assignments near the top of your copied `launch.sh`,
before its existing code, for example:

```sh
export SETTLERS7_BOTTLE='Your bottle name'
export CROSSOVER_WINE='/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine'
```

Setting a variable in an unrelated terminal does not affect an already running Steam.

## Tests and troubleshooting

```sh
sh tests/run.sh 'Your existing bottle name'
```

This generates synthetic audio and checks decoder boundaries, short reads,
seeking, malformed-input handling, DEP policy behavior, and launcher argument
forwarding. It does not launch Settlers or need a Miles DLL. The named bottle
must already exist and be initialized. See [test instructions](tests/README.md)
for the optional graphics and concurrent-audio reproductions.

The launcher replaces `Data/MacCompatibility/last-launch.log` on each launch.
The DEP helper emits an `OutputDebugString` diagnostic only when it attempts a
policy change; it creates no diagnostic file. To isolate the audio replacement
from the DEP change with Steam Play, add `SETTLERS7_DISABLE_DEP_FIX=1` to the
existing quoted `--env` list in your copied `launch.sh`.
The FPS counter comes from `DXVK_HUD=fps` in `launch.sh`; remove that setting if
you do not want it. Logs can contain local paths or account data: redact them
before posting an issue.

## Undo

Close the game. Restore the saved Steam Launch Options and original
`mssmp3.asi`. Restore a previously existing `d3d9.dll`, or move the added one
out of `Data/Base/Bin/`. Move the added `Data/MacCompatibility/` directory aside.
Restore your graphics options separately if desired. Saves are not modified by
the installer steps. No system setting needs restoring.

Steam verification or a game update may replace the codec. Recheck compatibility
before applying this experimental replacement to a different game version.

## Related work and license

[S7Patcher](https://github.com/Eisenmonoxid/S7Patcher) offers other Settlers 7
patches, including CPU-affinity changes.
[OpenMiles](https://github.com/maci0/openmiles) replaces the broader Miles sound
library. Neither was tested in this setup. This project keeps a narrower scope.

Original compatibility code is [MIT licensed](LICENSE). The experimental Wine
patches are LGPL-2.1-or-later and the FEX patch is MIT; their notices are included
in the [source bundle](experimental/arm64-fex/BUILD.md#licensing-and-provenance). Vendored minimp3 is
[CC0-1.0](vendor/minimp3/LICENSE). See [CREDITS.md](CREDITS.md) for provenance and
upstream references. This is an independent, unofficial compatibility project.
