# Graphics tuning with the experimental ARM64/FEX runtime

These measurements apply to the [patched ARM64/FEX setup](../experimental/arm64-fex/README.md),
with reduced x87 precision enabled. They do not describe the separate CrossOver
installation. Test hardware was an M1 Max with 32 GPU cores and 32 GB memory;
the game was History Edition 1.12.1395.0, using DXVK-Sarek 1.14.1 and the bundled
Highball MoltenVK. Testing was performed on 21 October 2026.

## Tested quality preset

This preset aims for approximately 60 fps in the small Forest Realm match used
for testing. It is a starting point, not a guaranteed minimum or a claim about
late-game settlements.

| Setting | Value |
| --- | --- |
| Resolution and mode | 1728 x 1080, 16:10, windowed |
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

The game's Upscaling setting is reported by its UI name. Its internal rendering
scale was not established. Back up your options before changing them, apply
settings, and reload the map when prompted. Check that the intended controls
and saved options actually changed.

For a little more headroom, keep this preset but use **Low clutter** and turn
**sun shafts and parallax mapping off**. That combination averaged about 64 fps
at the starting view, versus about 60 fps for the quality preset.

After a normal exit and fresh app launch, the quality preset and saved match
reloaded successfully, with the options file unchanged. A longer 45-second
stationary check averaged **61.1 fps** over 44 seconds of complete samples;
the two camera routes averaged **56.7 and 58.0 fps**. The longest stationary
display interval was 144 ms, so the average does not mean every frame was smooth.

## What made a material difference

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

Each option comparison reloaded the same named save. Gameplay readiness was
checked visually, and foreground ownership was sampled throughout. Native
Metal presentation statistics were collected for a 20-second stationary phase
and two short arrow-key camera routes. Results use complete one-second bins
inside each phase, about 19 seconds stationary and 6 to 7 seconds per route.
The final fresh-launch verification used a 45-second stationary phase instead.
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

The known intermittent loader crash and reduced-precision limitations described
in the [runtime notes](../experimental/arm64-fex/README.md) still apply. Large
settlements, multiplayer and long uninterrupted sessions have not been validated.
Raw captures, account state, local paths and saves are intentionally not included.
