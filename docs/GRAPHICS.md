# Graphics tuning with the experimental ARM64/FEX runtime

These measurements apply to the [patched ARM64/FEX setup](../experimental/arm64-fex/README.md),
with reduced x87 precision enabled. They do not describe the separate CrossOver
installation. Test hardware was an M1 Max with 32 GPU cores and 32 GB memory;
the game was History Edition 1.12.1395.0, using DXVK-Sarek 1.14.1 and the bundled
Highball MoltenVK. Testing was performed on 21 and 22 October 2026.

## Tested fullscreen quality preset

This preset aims for approximately 60 fps in the small Forest Realm match used
for testing. It is a starting point, not a guaranteed minimum or a claim about
late-game settlements.

| Setting | Value |
| --- | --- |
| Resolution and mode | 2056 x 1285, 16:10, macOS fullscreen |
| Shader quality | High |
| Textures, water and object detail | High |
| Shadows and clutter | High |
| Upscaling | High |
| View range | Medium |
| Antialiasing | On |
| Sky, terrain detail maps, clouds and fog | On |
| Ambient occlusion, sun shafts and parallax mapping | On |
| Particles | **Off** |
| Depth of field | Off |

On the test display, 2056 x 1285 fills the usable area in macOS fullscreen,
below the display's notch strip. Lower game resolutions can leave black margins
in this mode. This resolution is specific to the tested display configuration,
not a universal minimum for every Mac. Keep the game's saved `FullScreen` flag
at `0`, enter fullscreen with the macOS window control, then check the game's
Video Settings. On a later fresh launch, macOS fullscreen initially retained a
2056 x 1262 client. Reapplying 2056 x 1285 and 16:10 inside fullscreen expanded
the client to the requested size. Accept a resolution confirmation within
15 seconds if one appears. This manual workflow was verified; automatic
fullscreen is discussed below.

The game was checked visually across the display, and native Metal statistics
confirmed a 2056 x 1285 rendering surface. With the preset above, a 20-second
stationary test averaged **60.3 fps** over 19 seconds of complete samples.
The two short camera routes averaged **57.0 and 59.3 fps**. The longest display
interval in the retained measurement samples was 46 ms. These results followed
a successful reload of a later checkpoint in the same small Forest Realm match.

A later fresh-launch recovery check on the built-in display used the same
quality settings with **depth of field on**. It averaged **58.7 fps stationary**
and **58.4 / 60.4 fps** on the camera routes at a verified 2056 x 1285 Metal
surface. Stationary coverage was 95 percent; camera-route coverage was 97 and
84 percent. The lowest one-second stationary average was 43.9 fps and the
longest retained display interval was 70 ms. The restored options had depth of field off before launch, but it was on in the benchmark's
saved options; the step that re-enabled it was not isolated. These results
confirm playable recovery in the test match. They do not identify the cause of
the earlier severe slowdown or prove that it cannot recur.

The game's Upscaling setting is reported by its UI name. Its internal rendering
scale was not established. Back up your options before changing them, apply
settings, and reload the map when prompted. Check that the intended controls
and saved options actually changed.

## External display and the FullScreen option

**External fullscreen is still under investigation.** The tests below do not
establish a reliable external-display setup or a performance recommendation.

A later test used `FullScreen=0`, `ScreenWidth=1920`, `ScreenHeight=1080` and
16:9 (`AspectMode=4`) in the game's `Options.ini`. After moving the game to the
external display, the macOS fullscreen control filled its 1920 x 1080 logical
area. The panel's physical resolution was 3840 x 2160; the renderer reported a
1920 x 1080 buffer. Accept the game's resolution confirmation within 15 seconds
or it reverts. The tested change was accepted and the saved options verified.

This native fullscreen path also produced a black transition and later long
presentation stalls. Leaving fullscreen restored responsiveness in observed
cases, but this is recovery from the symptom, not a fix. A borderless-window
comparison has not established a reliable alternative. There is no valid
external-fullscreen average to compare with the laptop results above.

The borderless stall also reproduced with the original installed native driver.
During a 12-second check, the game stayed foreground, its start and end images
were identical, and the native driver logged no presentation calls. The problem
is therefore not limited to the locally rebuilt driver. A frozen FPS overlay
does not provide a valid frame-rate measurement for this interval.

A test with `FullScreen=1` opened on the primary laptop display despite the
intended external display. The game requested 1920 x 1200 after the options
had been set to 1920 x 1080. The old 16:10 aspect option was still present;
the reason for the mode substitution was not isolated. In the pinned Sarek build,
[fullscreen entry selects the default monitor](https://github.com/pythonlover02/dxvk-sarek/blob/0c0e8f6f62c7cb37dbf5d38413517efa00c08fef/src/d3d9/d3d9_swapchain.cpp#L1266),
which [maps to the primary display](https://github.com/pythonlover02/dxvk-sarek/blob/0c0e8f6f62c7cb37dbf5d38413517efa00c08fef/src/wsi/win32/wsi_monitor_win32.cpp#L9).
Its activation handler also minimizes the game on focus loss. A verified way
to select the external monitor for this exclusive mode is still missing.
Keyboard and pointer focus remain shared between both displays.

A subsequent primary-display test with `FullScreen=1`, 2056 x 1285 and 16:10
did automatically display the game fullscreen. During input checks, macOS
reported the game as foreground while its Windows window was minimized and a
different Windows process held focus. A normal Windows restore operation was
tried, and gameplay was later visible, but the test ended before input,
performance and focus-return verification were complete. The saved setting
was restored to the measured `FullScreen=0` configuration. A reliable automatic
fullscreen workflow with `FullScreen=1` therefore remains unverified.

Further built-in-display tests isolated a startup geometry mismatch. Before
native activation, the guest window and client were 2056 x 1329. Activation
changed the display mode to 2056 x 1285 while leaving the guest client and
Metal surface at 2056 x 1329. Correcting only the client restored visible menu
clicks and save loading, but left the larger Metal surface. That run averaged
59.7 fps stationary and 44.9 / 45.8 fps on camera routes, worse than the measured
manual fullscreen recovery above. Resizing the window alone is therefore not
the recommended fix.

Selecting the supported 2056 x 1285 display mode through native CoreGraphics
before starting the game kept the display, guest client and Metal surface at
2056 x 1285. Visible clicks, save loading, normal Command-Tab return, menu and
camera input, and normal exit with temporary-mode release all passed. Four
clicks in a short recording showed no Mac arrow in the inspected frames.
However, a fresh same-day comparison favored macOS fullscreen:

| Mode, same save and quality | Stationary | Camera route 1 | Camera route 2 |
| --- | ---: | ---: | ---: |
| Game fullscreen (`FullScreen=1`), native mode selected first | 53.8 fps | 39.2 fps | 47.7 fps |
| macOS fullscreen (`FullScreen=0`), manual resolution reapply | 61.8 fps | 59.3 fps | 62.0 fps |
| macOS fullscreen, automatic native entry and client-size correction | 61.9 fps | 58.7 fps | 61.3 fps |

All runs used the same High/Medium, particles-off preset with depth of field
**on** and verified 2056 x 1285 Metal surfaces. The true-fullscreen run differed
only in its FullScreen option. Stationary coverage was 95 percent; camera
coverage varied from 79 to 99 percent. Longest retained display intervals were
190 ms for true fullscreen, 37 ms for the manual control and 53 ms for the
automatic native-entry trial. These short sequential comparisons support
keeping `FullScreen=0`; they do not establish a universal performance ratio.

The automatic native-entry trial used Accessibility to enter macOS fullscreen,
then a narrowly targeted Win32 `SetWindowPos` operation to restore the initially
clamped 2056 x 1262 client to 2056 x 1285. It did not change the display mode or
require an in-game resolution reapply. Save loading, normal app switching, menu
clicks and camera movement passed. Optional helper sources and the separate
macOS permission requirement are described in the
[runtime notes](../experimental/arm64-fex/README.md#built-in-display-launcher).
The normal app launcher still needs an end-to-end check with its own permission.

The per-game Wine `EmulateModeset=Y` alternative failed before rendering with a
`make_ratio` assertion in `sysparams.c`; the test setting was removed. It is not
a working workaround for this tested configuration.

## Earlier windowed measurements

The following option comparisons used **1728 x 1080 windowed**. They identify
useful trade-offs, but do not establish their exact cost at the fullscreen
resolution above.

A headroom alternative at that resolution used **Low clutter** with
**sun shafts and parallax mapping off**. That combination averaged about 64 fps
at the starting view, versus about 60 fps with those extras enabled.

After a normal exit and fresh app launch at 1728 x 1080, the saved match reloaded
successfully. The windowed quality preset with High clutter, sun shafts and
parallax enabled persisted, with the options file unchanged. A longer 45-second
stationary check averaged **61.1 fps** over 44 seconds of complete samples;
the two camera routes averaged **56.7 and 58.0 fps**. The longest stationary
display interval was 144 ms, so the average does not mean every frame was smooth.

### What made a material difference

**Particles were the clearest expensive option.** With High shader, texture,
water, object and shadow settings, Medium view range, Low clutter, antialiasing
and ambient occlusion on, and depth of field, sun shafts and parallax off:

| Particles | Stationary average | First camera route | Repeated camera route |
| --- | ---: | ---: | ---: |
| On | 45.6 fps | 37.3 fps | 38.2 fps |
| Off | 63.8 fps | 56.5 fps | 60.7 fps |

The saved option files differed only in the Particles flag. Turning it off
removes effects such as fire and smoke. This is a graphics trade-off, not a
binary fix for the particle renderer.

Shadows also cost substantial performance. In an earlier Medium-shader test
with High object detail and Low water/view range, enabling Low shadows reduced
the stationary average from 84.8 to 69.3 fps. The later quality preset retains
High shadows for their appearance. A separate High-versus-Low shadow comparison
was partly confounded by a diagnostic process and is not a reliable exact cost.

High object detail, antialiasing and ambient occlusion did not show a meaningful
stationary penalty in their individual comparisons in this small scene. High
view range was more expensive than Low in the sequential tests, so Medium is
used here. A background-process cleanup occurred between those view-range
runs; their exact difference cannot be attributed solely to the setting.

Enabling High clutter, sun shafts and parallax together on the particles-off
configuration changed the stationary result from 63.8 to 60.0 fps. The two short
camera routes averaged 54.3 and 56.0 fps with these extras. Their separate costs
were not measured.

## Measurement and limits

Each windowed option comparison reloaded the same named save. The fullscreen
follow-up preserved and reloaded a separate later checkpoint; loading resets
the camera to the castle. Its results are not a controlled comparison of
resolution alone. Gameplay readiness was
checked visually, and foreground ownership was sampled throughout. Native
Metal presentation statistics were collected for a 20-second stationary phase
and two short arrow-key camera routes. Results use complete one-second bins
inside each phase, about 19 seconds stationary and 6 to 7 seconds per route.
The earlier windowed fresh-launch verification used a 45-second stationary
phase instead. A fullscreen trial that overlapped helper compilation was
excluded from the recommended preset's measurement.
These are short sequential observations, not repeated benchmark confidence
intervals. The camera path drifts slightly and bin coverage differs between
passes, so the two routes do not prove a precise cache-warming benefit.

Averages are counted presented frames divided by sampled duration, not readings
from a single HUD screenshot. The lowest one-second average is not an
instantaneous minimum or a 1% low. Two early runs without verified load/menu
readiness were excluded, as were profiling runs from normal FPS comparisons.

Occasional long display intervals remain unresolved. Earlier configurations
showed individual intervals around 100 to 300 ms. Native profiling did not
establish a shader-compilation, FEX-compilation, autosave or recovered-wait cause.
The requested Metal compiler-event data was absent, which limits that diagnosis.
Existing DXVK and MoltenVK caches were already populated. No speculative cache
or synchronization change was applied, and this guide does not claim to fix all
stutter.

A brief macOS-arrow flash after left-click was later reproduced in continuous
video. Earlier delayed screenshots missed it. A ten-second recording with four
stationary native clicks in macOS fullscreen showed the arrow after every click;
one click was held for half a second. Simultaneous Wine cursor tracing mapped
the requested shapes to the game's hand and fist resources, with no system-arrow
request during
those clicks. This supports investigating native cursor restoration, but does
not by itself establish a fix. Later windowed recordings with both an experimental
cursor patch and an unmodified control showed no arrow in four clicks each.
The windowed result therefore does not prove that the patch fixes the fullscreen
bug. No cursor patch is included in this release. Capture frame rates are not
game-performance measurements.

Severe lag remains unresolved. External fullscreen tests later reproduced long
presentation gaps while macOS reported the game as foreground. Separate runs
that lost foreground ownership were excluded. Successful short FPS measurements
do not establish that these stalls are fixed.

The known intermittent loader crash and reduced-precision limitations described
in the [runtime notes](../experimental/arm64-fex/README.md) still apply. Large
settlements, multiplayer and long uninterrupted sessions have not been validated.
Raw captures, account state, local paths and saves are intentionally not included.
