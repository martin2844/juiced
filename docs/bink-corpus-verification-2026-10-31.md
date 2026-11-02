# Bink video corpus verification — 2026-10-31

The production native CPU decoder matches an independent FFmpeg decoder for
**all 472 supplied Bink clips and all 88,631 frames**. Every frame has equal
SHA-256 over its visible-sized, original-resolution planar Y/U/V bytes. This
covers 20,123,365,248 decoded bytes by frame hashes; it is not a direct byte
comparison of that whole volume. Three pilot raw frames were also compared
directly, and their SHA implementation was checked with Python hashlib.

This strengthens the investigation of talking-head video, but does not certify
its loading time, UI placement, YUV-to-RGB conversion, scaling, GPU upload,
audio, long-run A/V sync or Android/ARM64 behavior. No production fix or rebuild
was needed for this result. No new claim about 60 FPS is made.

## Inputs, decoder identities and coverage

Evidence: `out/validation/linux-current-20261029/bink-corpus-r14/`.
`corpus-plan.json` independently binds every original path, file SHA, size and
binary header. All 1,314,462,712 source bytes remain unchanged. All supplied
clips are BIKi, with no alpha flag: 450 at 320x192, 19 at 640x480, and one each
at 800x450, 720x480 and 1024x768. Rates are 25/1 (19), 2997/100 (451), and
30/1 (2). Untested Bink versions/flags and arbitrary external files are not
covered by this corpus.

`BinkProbe.cpp` is an out-only CPU adapter calling the actual production
`MovieHeader` and `VideoDecoder` interfaces. It sequentially decodes all video
packets, crops stride padding, and hashes every frame's canonical planes.
It skips audio packets without decoding them. It does not initialize the game,
SDL, a GPU, an audio sink or a device. `build.py` reuses current compile commands
and existing production static libraries; full commands/library identities are
retained in `build-receipt.json`. Production libraries were not rebuilt.

* Production `src/Game/Media/BinkNative.cpp` SHA-256:
  `b275492cf73027b90f512df8184bdbbd7fb4ed37f1096a520639a7ec698453b7`.
* CPU adapter executable SHA-256:
  `ec1803c66f11304d802aff225b5979f8883c197046eebb897efe9d6518de1542`.
* Independent retained FFmpeg 8.1.2 executable SHA-256:
  `c43fc7b7271d2df8bbebc2445dc956406a60a0015fc6a7ad27ac959415ac1e40`.

FFmpeg runs a separate decoder on each original file, with original yuv420p
output and per-frame SHA-256 via framehash. It is a local reference tool; it
was not added to the product or distribution. The production restricted LGPL
audio configuration is unchanged. Native and reference output hashes, exact
argv, logs and both terminal receipts are retained for each `corpus/clip-*`.

## Calibration and independent verification

The first Biggi pilot matched all 164 frames; three extracted native YUV frames
matched the independent raw reference byte for byte. Five copied-output
negatives reject one changed digest, a missing frame, an extra frame, wrong
ordering and all-zero frames. Five actual malformed input copies reject a short
header, short table, wrong signature, out-of-file index and truncated video
packet; the latter also observes a native decode failure. These are bounded
negative controls, not a general malformed-file security audit.

`run_corpus.py` stops on the first mismatch. All 472 cases completed, with zero
held-empty packets, zero differing frame hashes and all 944 decoder/reference
subprocesses successful. The runner ended 0 in 199.690 seconds. `verify.py`
separately rereads every original file hash/header, native/reference output hash,
all frame hashes, plane sizes/counts, subprocess terminals and build/library
identities. `verification-receipt.json` binds the final result and ten negative
controls. Existing pilot, build and calibration evidence is retained.

All 908 critical production inputs, the development ELF, six personal Linux
save/creative files and every installed Wave2 file remain unchanged. No RP5
access, APK/install, original movie edits, personal save edits or public upload.
Checkpoints contain helpers, receipts and numeric hashes, excluding original
clips, malformed copied clips, raw planes, user saves and compiled binaries.
The broader verification goal remains active.
