# Credits and provenance

- **minimp3**, by lieff and its contributors, supplies the MP3 decoder. The
  unmodified `minimp3.h` and CC0-1.0 license are pinned to
  [ea99364f61c14656440e8d77e9c233ccf3124633](https://github.com/lieff/minimp3/commit/ea99364f61c14656440e8d77e9c233ccf3124633).
  See [vendor details](vendor/minimp3/README.md).
- **athei and Highball** documented the DEP/Rosetta penalty for GPU-visible
  writable/executable memory. Highball's
  [issue 165](https://github.com/gauthierpiarrette/highball/issues/165) contains
  the investigation, and
  [0.10.5](https://github.com/gauthierpiarrette/highball/releases/tag/v0.10.5)
  incorporates the Wine-engine fix. This project reproduces the problem in
  Settlers and applies a small process-local workaround for stock CrossOver;
  it does not claim the original discovery or include their patch code.
- **DXVK-Sarek** provides the D3D9-to-Vulkan renderer used in testing:
  [version 1.14.1](https://github.com/pythonlover02/dxvk-sarek/releases/tag/v1.14.1).
  **MoltenVK** supplies Vulkan over Metal:
  [version 1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2).
  Neither library is vendored or modified here.
- **Wine and CodeWeavers CrossOver** provide the Windows compatibility runtime.
  The launcher uses Wine's internal `__wine_unix_spawnvp` and
  `wine_get_unix_file_name` exports. These are runtime-specific interfaces,
  so future Wine/CrossOver versions may require changes.

The adapter and test clients contain handwritten interface declarations and
original implementation code. ASI/RIB calling conventions were checked against
publicly visible Miles interface headers and the local Miles 7.2g runtime.
The proprietary SDK headers and implementation are not included or relicensed.
The original `mss32.dll`, `mssdsp.flt`, codec, game executable, audio assets,
disassembly, saves, screenshots and machine logs are not part of this release.

Miles Sound System and the game remain the work of their respective owners.
This project has no affiliation with or endorsement from those owners.

## ARM64/FEX experiment

The [experimental bundle](experimental/arm64-fex/README.md) builds on
Highball's ARM64 r6 integration, Wine 11.18 and FEX 2610. Its CEF compatibility
patch adapts CodeWeavers' CW HACK 25737 from the published CrossOver 26.3
sources. Wine-derived patches retain LGPL-2.1-or-later licensing; FEX's MIT
notice is included. Exact source pins, patch order, licenses and the distinction
between observed fixes and unresolved hypotheses are documented in
[BUILD.md](experimental/arm64-fex/BUILD.md).
