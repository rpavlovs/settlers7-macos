# Graphics tuning with the experimental ARM64/FEX runtime

These measurements apply to the [patched ARM64/FEX setup](../experimental/arm64-fex/README.md),
with reduced x87 precision enabled. They do not describe the separate CrossOver
installation. Test hardware was an M1 Max with 32 GPU cores and 32 GB memory;
the game was History Edition 1.12.1395.0, using DXVK-Sarek 1.14.1 and the bundled
Highball MoltenVK. Testing was performed on 21 October 2026.

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
not a universal minimum for every Mac. Use the macOS window fullscreen control;
the game's saved `FullScreen` flag remained `0` in this test. Wine's separate
exclusive fullscreen mode was not validated.

The game was checked visually across the display, and native Metal statistics
confirmed a 2056 x 1285 rendering surface. With the preset above, a 20-second
stationary test averaged **60.3 fps** over 19 seconds of complete samples.
The two short camera routes averaged **57.0 and 59.3 fps**. The longest display
interval in the retained measurement samples was 46 ms. These results followed
a successful reload of a later checkpoint in the same small Forest Realm match.

The game's Upscaling setting is reported by its UI name. Its internal rendering
scale was not established. Back up your options before changing them, apply
settings, and reload the map when prompted. Check that the intended controls
and saved options actually changed.

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

Cursor changes to the macOS arrow after clicking and transient severe lag have
also been reported in macOS fullscreen. Bounded automated left- and right-click
tests retained the custom game cursor, and subsequent foreground samples did
not reproduce continuous severe lag. Both reports remain unresolved; the
successful short FPS measurements do not establish that they are fixed.

The known intermittent loader crash and reduced-precision limitations described
in the [runtime notes](../experimental/arm64-fex/README.md) still apply. Large
settlements, multiplayer and long uninterrupted sessions have not been validated.
Raw captures, account state, local paths and saves are intentionally not included.
