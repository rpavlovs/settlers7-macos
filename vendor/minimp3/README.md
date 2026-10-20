# Vendored minimp3

Source: [lieff/minimp3](https://github.com/lieff/minimp3).
Pinned revision:
[ea99364f61c14656440e8d77e9c233ccf3124633](https://github.com/lieff/minimp3/commit/ea99364f61c14656440e8d77e9c233ccf3124633).

Both files are unmodified. SHA-256:

```text
57e437c5c1f0e8b243885d3929c8973b5e6c778451e0100ab4251d19915cb3ad  minimp3.h
6a1ee543e5282cd9061881edf462e6fdab181f328da71fc2c9a6950a80e94d01  LICENSE
```

The header is compiled with `MINIMP3_ONLY_MP3` and `MINIMP3_ONLY_SIMD` using
SSE2 code generation for the 32-bit Windows target. The adapter relies on
internal header helpers, so updating this pin needs a build and decoder test.
