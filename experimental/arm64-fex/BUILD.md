# Source and build reproduction

These are the source inputs and commands used for the local experiment.
They are not a turnkey fresh-install script. The original pinned builds and
focused runtime tests passed; this normalized documentation has not been run
as a clean, end-to-end installation on another Mac.

## Pinned inputs

- Highball engine sources: [fc628ae108aead7270812088a38b9ef942ba6970](https://github.com/gauthierpiarrette/highball-engine/commit/fc628ae108aead7270812088a38b9ef942ba6970).
  Use this revision's `inputs-arm64.json`, ARM64 build workflow and overlay lists.
- Wine 11.18 source archive SHA-256:
  `c6282f6d4daecf11f3ab9f9a2b6c598f67be7e28c363ba2cae8a3c44b611e7a4`.
- FEX source: [14c92681f4d62cf84d901460e0358de09c8847a7](https://github.com/FEX-Emu/FEX/commit/14c92681f4d62cf84d901460e0358de09c8847a7),
  tag FEX-2610. Initialize the six submodules named in `inputs-arm64.json` at
  the commits recorded by that FEX revision.
- llvm-mingw 20260922, macOS universal, clang 23.1.2, archive SHA-256:
  `52e5f5a7b131021d0c39a37a38fa380a1da7885cd04bd61afd0cd4ecfb8bc1f3`.
- Native compilation used Apple clang and the macOS 27 SDK. Compatibility of
  these locally built binaries with older macOS releases is untested.

Prepare Wine with the **25 `patches-arm64/*.patch` files selected by the r6 upstream
workflow**, then apply this directory's three `patches/wine/*.patch` files in
numeric order. Do not apply all Wine patches in the upstream repository;
that also contains patches for other engine variants.
Prepare FEX with its **23 r6 `fex/patches/*.patch` overlays** in lexical order,
then apply `patches/fex/0024-clear-stale-dep-promotion.patch`.
An unmodified r6 control build is useful before applying each candidate.

## Wine components

Use absolute paths below. `WINE_SRC` is the prepared Wine tree;
`LLVM_MINGW` is the extracted pinned toolchain. Build in empty directories.

```sh
export PATH="$LLVM_MINGW/bin:$PATH"
mkdir wine-unix-build
cd wine-unix-build
"$WINE_SRC/configure" --enable-archs=none --disable-tests \
  --without-x --without-alsa --without-capi --without-cups --without-dbus \
  --without-fontconfig --without-gphoto --without-gstreamer --without-krb5 \
  --without-netapi --without-oss --without-pulse --without-sane --without-udev \
  --without-usb --without-v4l2 --without-pcap --without-ffmpeg \
  CC=/usr/bin/clang CXX=/usr/bin/clang++
make -j2 dlls/ntdll/ntdll.so
cd ..
mkdir wine-pe-build
cd wine-pe-build
"$WINE_SRC/configure" --enable-archs=arm64ec,aarch64,i386 --with-mingw \
  --disable-tests --without-x --without-alsa --without-capi --without-cups \
  --without-dbus --without-fontconfig --without-gphoto --without-gstreamer \
  --without-krb5 --without-netapi --without-oss --without-pulse --without-sane \
  --without-udev --without-usb --without-v4l2 --without-pcap --without-ffmpeg \
  CC=/usr/bin/clang CXX=/usr/bin/clang++ \
  arm64ec_CC=arm64ec-w64-mingw32-clang \
  aarch64_CC=aarch64-w64-mingw32-clang i386_CC=i686-w64-mingw32-clang
make -j2 dlls/ntdll/i386-windows/ntdll.dll
```

Only these selected components were replaced in the private runtime. The
minimal configure options above are not a recipe for replacing a full Wine
installation or its other libraries.

## FEX component

`FEX_SRC` is the prepared FEX tree. CMake 4.4.3 with Unix Makefiles was used;
the upstream CI uses Ninja. The compiler, target and options were retained.

```sh
export PATH="$LLVM_MINGW/bin:$PATH"
cmake -G 'Unix Makefiles' -S "$FEX_SRC" -B fex-build \
  -DCMAKE_TOOLCHAIN_FILE="$FEX_SRC/Data/CMake/toolchain_mingw.cmake" \
  -DMINGW_TRIPLE=aarch64-w64-mingw32 -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_LTO=False -DBUILD_TESTING=OFF -DENABLE_ASSERTIONS=OFF \
  -DBUILD_FEXCONFIG=OFF -DTUNE_CPU=generic -DTUNE_ARCH=generic
cmake --build fex-build --target wow64fex --parallel 2
```

The `libwow64fex.dll` output is installed as `xtajit.dll`. Do not substitute
an ARM64EC DLL or change `xtajit64.dll`; this game is a 32-bit x86 process.
The local unmodified control was not byte-identical to the released DLL,
so both were functionally tested and both reproduced the DEP defect.

## Private deployment targets

First close the game and stop **only the isolated test prefix** completely.
Preserve each original file before replacing it. Keep the signed loader intact.

| Output | Locations relative to the private runtime root |
| --- | --- |
| Native `ntdll.so` | `engine/lib/wine/aarch64-unix/ntdll.so` |
| PE32 `ntdll.dll` | `engine/lib/wine/i386-windows/ntdll.dll`; `prefix/drive_c/windows/syswow64/ntdll.dll` |
| `libwow64fex.dll`, renamed `xtajit.dll` | `engine/lib/wine/aarch64-windows/xtajit.dll`; `prefix/drive_c/windows/system32/xtajit.dll` |

The resulting locally tested SHA-256 values were:

```text
ntdll.so   ba206e642a65ead830f05aa2b7bb8a4f93f5d57dd6f73808e18b5faa9fa0fa58
ntdll.dll  9fabc8528112ac8d26fb2e3b424fe058295037a7fc36b6493be2550f5a70c674
xtajit.dll e13b459fcbdd8134cf98cb52d7368f84585c9babc00ffab427236cde3f89664b
```

Build paths, timestamps and toolchains can change binary hashes. These identify
the tested local artifacts, not guaranteed reproducible output checksums.
The published wait-recovery patch clarifies an uncertainty in a comment;
its executable statements match the installed candidate.

## Focused tests

Build the owned-process reproductions with a 32-bit MinGW compiler:

```sh
i686-w64-mingw32-gcc -O2 -Wall -Wextra -Werror \
  tests/settlers-dep-transition-test.c tests/settlers-dep-transition-test.S \
  -o settlers-dep-transition-test.exe
i686-w64-mingw32-gcc -O2 -Wall -Wextra -Werror \
  tests/settlers-alert-loss-test.c -o settlers-alert-loss-test.exe
```

Run each through the private runtime's `run.py`, using its absolute Windows
`Z:` path. The DEP test takes no arguments. It should pass all seven cases on
the candidate and fail the explicit-RX transition on stock r6.
The alert test takes `baseline` or `candidate`, matching the runtime under
test. It deliberately consumes one selected wake in its **own process**, then
checks recovery, normal wake and a finite timeout. It fingerprint-checks its
Wine PE32 syscall stub and refuses a different layout. It restores the stub
and joins its workers on the successful test path. It is not a general Wine
conformance suite.

Other local checks covered image writes/protections, exception and register
behavior, instruction-cache invalidation and image-tail handling. Those plus
a passing probe do not replace the actual game's login, rendering, input,
audio, save/load and exit workflow. See the [observations](README.md).

## Licensing and provenance

The three Wine patches are **LGPL-2.1-or-later**, consistent with the files
they modify; see [license](licenses/LGPL-2.1.txt). The CEF adaptation derives
from CodeWeavers' CW HACK 25737 in the
[CrossOver 26.3 source archive](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz).
The other two patches extend the pinned Highball/Wine source.

The FEX patch is **MIT**; retain the [FEX notice](licenses/FEX-MIT.txt).
The standalone test clients and `run.py` are covered by the repository's
[MIT license](../../LICENSE). No compiled runtime, game library, CEF binary,
account data, save, screenshot or machine log is distributed here.
