# Validation and limits

## Test environment

The initial local work used an Apple M1 Max with 32 GB RAM, macOS 27.0.1,
CrossOver 26.3.0, the Steam History Edition of Settlers 7 (1.12.1395.0),
Miles 7.2g, DXVK-Sarek 1.14.1 and the regular MoltenVK 1.4.2 macOS library.
The Steam bottle was Windows 10, 64-bit, with Auto graphics, MSync enabled and
high-resolution mode disabled.
The following tests were performed on 20 October 2026.

## Isolated findings

| Check | Original configuration | Local compatibility build |
| --- | --- | --- |
| 16 simultaneous generated MP3 streams, roughly 10 seconds of audio | Only 4.312 seconds advanced in 14.010 wall seconds; worst position/status query blocked for 2995.780 ms | Completed in 10.142 seconds; worst query 0.770 ms |
| Fresh 512×512 managed D3D9 texture LockRect with the same Sarek/MoltenVK libraries | DEP disabled: 2162.93 ms | Process-local DEP correction: 0.7115 ms |

A PCM control completed 16 streams in 10.138 seconds, with a 0.198 ms worst
query. Plain allocation/memset tests did not reproduce the graphics stall;
the large penalty appeared on the graphics-mapped resource path. Native memory
inspection showed the relevant mapping change from RWX to RW after correction.
The controlled results identify two bottlenecks, not a general performance
guarantee or an average game FPS improvement of the same magnitude.

Decoded output at 22.05 kHz mono, 44.1 kHz stereo and 48 kHz stereo matched the
original codec's rates, channels and frame counts. PCM was close, not
bit-identical. Concurrent Miles playback, ordinary seeking and looping passed
separate tests with the original local Miles library.

## Gameplay evidence

The earlier installed variant launched from Steam through Ubisoft Connect,
loaded a saved Forest Realm match, moved the camera, placed a lodge, connected a
road, delivered construction materials and completed the building. It exited
normally and Steam stopped marking the game as running.

Foreground HUD snapshots included 13.2, 17.0, 20.6, 21.8, 23.3, 24.5, 25.7,
27.4, 28.8 and 29.9 fps. These were observations during play, not a benchmark
average. The animated menu remained slow. Application-only audio capture was
continuous and non-silent over two roughly 13-second samples; this is a measured
capture result, not a subjective audio-quality assessment.

## Source-release checks

The cleaned release adds a Wine-only guard and a DEP opt-out, removes a fixed
diagnostic-file path, changes the provider display name, and makes the launch
script inherit the actual bottle and CrossOver installation. It does not change
the decoder algorithm or DEP flag transformation.

The source builds with MinGW-w64 GCC and `-Wall -Wextra -Werror`. Generated-audio
tests pass at all three rates with output requests of 1, 3, 511, 4096 and 8191
bytes, 17-byte short reads, output canaries, seek-to-zero, external reposition
plus seek(-1), invalid process arguments, malformed bytes and invalid callback
results. The built DLL's export and DEP behavior pass disabled, enabled,
permanent and opt-out policy cases, including repeated initialization. Four
launcher shell tests pass, including spaces, literal shell metacharacters,
missing inputs, bottle selection and exit-code propagation.

A fresh private integration test of the cleaned plugin with the original Miles
runtime completed 16 generated MP3 streams (10.034 seconds of audio) in 10.123
wall seconds, with a 0.427 ms maximum query latency. The cleaned Windows
launcher also started an isolated graphics test in a directory with spaces;
it exited successfully and logged the expected Sarek version and settings.
These checks used private copies, leaving the installed game unchanged.

The exact cleaned release has not repeated the full gameplay test above.
The earlier local build's gameplay evidence must not be read as a full
cross-machine or long-session validation of this release.

## Known limits

- This is a narrow Miles 7.2g adapter, not a complete ASI implementation.
- Output is native-rate signed 16-bit mono/stereo PCM. Miles retains mixing,
  resampling and channel conversion responsibilities.
- Non-Layer-3 data, free-format MP3 and midstream rate/channel changes are rejected.
- Tests use CBR fixtures. VBR-specific timing, comprehensive malformed-input
  fuzzing and every temporary-starvation/metadata combination are untested.
- The undocumented seek(-2) behavior is inferred from observations. Its full
  contract is not established. After external seek(-1)/seek(-2), the low-level
  reported byte position is relative because the callback exposes no absolute cursor.
- The CUSTOM "Emitted samples" property assumes an eight-byte output buffer.
  The original property's complete size contract is unverified. This adapter
  writes a 64-bit count; callers must provide eight bytes. No use of this property
  was established in the tested game path.
- A zero-length source callback midway through an ID3 tag is treated like EOF
  at that point. If input resumes later, remaining tag bytes can be scanned as
  audio. General streaming/tag-starvation support is not established.
- Calls on a single stream handle must be serialized by the host. The adapter
  does not add a lock around decoding and property reads on that same handle.
- ID3 tag size and no-frame scans are bounded at four MiB per scan.
- A component that executes code from memory declared writable but not executable
  could fail with DEP enabled. The tested game path worked; other paths are untested.
- Native Windows, Intel Macs, other Apple GPUs, other game/Miles versions, larger
  maps and prolonged play have not been validated.

## Native ARM64/FEX follow-up, 21 October 2026

A separate Highball ARM64 r6 prefix used the exact cleaned release MP3/DEP
adapter during actual gameplay. The [experimental source bundle](../experimental/arm64-fex/README.md)
records four additional runtime patches, full versus reduced x87 observations,
construction and save/reload verification, launch/exit behavior, and one
unresolved menu crash. It used bundled Highball MoltenVK, not the CrossOver
library above. This extends gameplay evidence to that specific patched FEX
configuration; it does not establish the cleaned CrossOver launcher's full
gameplay validation or cross-machine/long-session stability.
