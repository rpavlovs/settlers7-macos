# Experimental native ARM64 Wine / FEX

These source patches and launch settings made Settlers 7 History Edition
1.12.1395.0 playable in an isolated Highball ARM64 r6 installation on an
M1 Max. This is a developer reproduction, not an automated installer or a
claim of support by Highball, Wine, FEX or Ubisoft.

**Known unresolved crash:** one run froze in `LdrShutdownThread` while a
DirectSound worker was exiting, before loading a save. Three subsequent
launches loaded the saved match, including two normal Steam app launches.
The crash cause is unknown. Long sessions, large settlements and multiplayer
remain untested.

## Tested result

With the patches below and the repository's release MP3/DEP adapter:

| Configuration | Menu HUD observations | Small saved-match HUD observations |
| --- | --- | --- |
| Full x87 precision | about 1.5–3 fps | about 9–10 fps |
| `FEX_X87REDUCEDPRECISION=1` | about 77–85 fps | about 90–98 fps stationary |

These are individual foreground HUD observations, not benchmark averages.
Camera transitions briefly showed about 28–57 fps. The test used a 1280×720
window, Low shader/object/water/clutter/view-range settings, High textures,
antialiasing, shadows, SSAO and particles disabled. Saved depth-of-field, fog
and terrain-detail flags remained enabled, although the Low shader tier can
suppress higher-tier effects. These settings are a tested baseline, not an
optimized graphics recommendation. See the later
[graphics tuning measurements](../../docs/GRAPHICS.md) for a tested 2056x1285
macOS fullscreen quality preset and earlier particle/shadow comparisons.

Camera movement, lodge construction, material delivery, a new named save,
reload after a fresh launch, normal exit and reopening while Steam remained
running all worked. The lodge remained completed after reloading. An
application-only 12.74-second audio capture was continuous and non-silent,
with no timestamp dropout or capture error. This was not a listening test.

**Precision trade-off:** the FEX option selects 64-bit floating-point handling
for x87 instead of the full-precision path. It changes numerical behavior.
A successful small single-player match does not establish simulation accuracy,
multiplayer compatibility or long-session stability.

## Patches and boundaries

Apply these only to the pinned, already patched Highball r6 sources described
in [BUILD.md](BUILD.md), not directly to an arbitrary Wine/FEX release.

| Patch | Change | Evidence and limitation |
| --- | --- | --- |
| `0044` Wine | Omit host EXEC permission for non-hybrid i386 image mappings that FEX executes by translation; retain guest protections | Corrected server-mediated image writes used by Ubisoft startup. Write/protection, code-invalidation and image-tail checks passed in the tested 4 KiB host-page mode. The 16 KiB fallback is untested. |
| `0045` Wine | Adapt CodeWeavers' CEF 135.0.20 command-line compatibility handling | Exact in-memory signature and bounds checks let requested software-rendering flags take effect. It is version-specific and does not change the on-disk CEF file or authentication checks. |
| `0046` Wine | Recheck completed wake selection in 100 ms slices for infinite i386-on-ARM64 address waits | Deterministic consumed-alert reproduction recovers, normal wakes and finite timeouts pass. The real alert consumer is not proven. Idle waiters can wake up to ten times per second. |
| `0024` FEX | Clear stale DEP-promotion metadata when explicit execute permission arrives | Stock and unmodified rebuilt controls lose explicit RX permission after a DEP transition; the candidate passes seven cases while still denying real non-executable pages. |

The game executable, DRM, account authentication and macOS security settings
are not patched. Steam and Ubisoft sign-in remain required. Keep this in a
separate ARM64 prefix; do not convert or overwrite a CrossOver bottle.

## Runtime and launch setup

The tested base is Highball **0.10.22**, engine
[ARM64 Wine 11.18 r6](https://github.com/gauthierpiarrette/highball-engine/releases/tag/engine-arm64-wine-11.18-20261020T101157Z-sdl3-link-r6),
FEX **2610**, and the engine's matching renderer/framework components.
Use the upstream signed Wine loader and its documented runtime layout; do not
remove signing, quarantine, or entitlement requirements to make a loader run.

The game uses the repository's built `mssmp3.asi` and **32-bit DXVK-Sarek
1.14.1 `d3d9.dll`**, with the **bundled Highball MoltenVK**. This differs from
the separate MoltenVK 1.4.2 library used in the CrossOver instructions.
Do not use the CrossOver `Settlers7-Mac.exe` launcher in this prefix.

`run.py` is the tested environment template. Place it in an existing private
runtime directory containing `engine/`, `prefix/`, `renderers/`, and
`frameworks/`. It does not create that installation, copy accounts, install
patches, or add registry settings. It derives paths from its own location and
passes an allowlist of ordinary host environment variables to Wine.

The tested prefix also has these per-executable Wine registry settings:

```text
HKCU\Software\Wine\AppDefaults\Settlers7R.exe\DllOverrides
  d3d9 = n,b
  gameoverlayrenderer = ""
  gameoverlayrenderer64 = ""
  overlay = ""
  overlay64 = ""
HKCU\Software\Wine\AppDefaults\upc.exe
  CommandLineAppend = --no-sandbox --in-process-gpu --disable-gpu
HKCU\Software\Wine\AppDefaults\steamwebhelper.exe
  CommandLineAppend = --disable-gpu --disable-gpu-compositing --in-process-gpu --no-sandbox --disable-gpu-sandbox
```

The browser flags disable Chromium's sandbox in these Wine launchers. They
were used for compatibility and are not a security improvement. Use the
launchers for their intended game/account flows, not general web browsing.
No system-wide security setting is changed.

Start Steam through this environment, then use Play, or run:

```sh
python3 /path/to/private-runtime/run.py \
  'C:\Program Files (x86)\Steam\steam.exe' -applaunch 965320
```

An already running Steam/Ubisoft process retains its old environment. Restart
only the private session when changing environment settings. The test template
sets reduced x87 precision for all its launched processes, including launchers.
A macOS app wrapper should start this command detached and return; keeping the
wrapper alive as Steam can make later app opens merely focus Steam.

### Built-in display launcher

`fullscreen-launcher.m` and `resize-client.c` automate the faster macOS
fullscreen path with the game's `FullScreen=0`. The native helper enters macOS
fullscreen, and the Windows helper corrects the client size that Wine initially
clamps below the requested height. The helpers do not change display modes,
graphics quality, game files, Wine libraries or saves. See the
[graphics results and limits](../../docs/GRAPHICS.md).

The existing runtime must have `run.py`, `logs/`, and `bin/`. Build the native
helper with Apple's command-line developer tools and the Windows helper with
a 32-bit MinGW-w64 compiler:

```sh
clang -fobjc-arc -Wall -Wextra -Werror \
  -framework AppKit -framework CoreGraphics \
  fullscreen-launcher.m -o fullscreen-launcher
i686-w64-mingw32-gcc -Wall -Wextra -Werror -O2 -static-libgcc \
  resize-client.c -o resize-client.exe
```

Place both outputs in the private runtime's `bin/` directory. Run:

```sh
/path/to/private-runtime/bin/fullscreen-launcher /path/to/private-runtime \
  '/path/to/private-runtime/prefix/drive_c/users/USERNAME/Documents/THE SETTLERS 7 - History Edition/Options.ini'
```

The tested configuration uses `FullScreen=0`, `ScreenWidth=2056`,
`ScreenHeight=1285`, and 16:10 (`AspectMode=5`). The built-in display must be the
unmirrored primary display. Requested client dimensions must fit its work area.
The helper validates the game's native process and Windows executable path;
the Windows check expects Steam's standard installation path shown above.
A different library location requires adapting that check deliberately.

**Permission:** the native helper needs macOS Accessibility access. On the
tested macOS version, the list is named **Device Control and Data Access**
under Privacy & Security. This OS permission permits broad app control even
though the source targets only Settlers. The helper requests permission through
the standard macOS prompt. Without it, the game still launches normally in a
window. Grant access only if comfortable with the source. Wait for the game
window to appear, then reopen the launcher. Reopening before Steam finishes its
pending launch can submit another launch request. Permission granted to a
terminal or controller does not automatically apply to a separately launched
app.

The helper waits up to three minutes for the actual game window, sizes it, then
exits. It does not remain running during play. Steam and Ubisoft manage their
own lifetimes. If they open the game after the timeout, reopen the launcher.
A lock avoids concurrent setup attempts. Reopening a running game requests
activation and reapplies sizing; it does not start another game instance.
macOS can ignore background activation requests. Command-Tab was verified for
normal user switching.

A macOS app wrapper should detach the helper with `nohup`, log its output
privately and return. Read that log if setup fails. A resize failure leaves the
game open for manual correction; the helper does not stop Steam or the game.
To remove this optional integration, restore the wrapper's normal `run.py`
Steam launch command and remove the two helper binaries. The runtime and saves
are independent of these helpers.

The same startup operations were verified from an authorized controller,
including save loading, exact client/rendering dimensions, gameplay and
Command-Tab. A normal app launch correctly exposed the separate permission
requirement and launched windowed without it. Normal app startup with its own
permission is still pending verification.

Keep the runtime outside temporary storage. Keep private logs and account
state out of public reports. Updates to Wine/FEX/CEF require a new compatibility
check; Steam verification or a game update can replace the custom codec.
See [BUILD.md](BUILD.md) for source pins, build targets, tests and licenses.
