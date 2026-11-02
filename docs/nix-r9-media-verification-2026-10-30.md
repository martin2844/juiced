# Linux r9 media dependency verification — 2026-10-30

The current Linux development launcher and Nix candidate now use a shared
FFmpeg override disabling GPL, version-3 and nonfree components. The preceding
Linux r8 candidate used the pinned package set's GPLv3-enabled FFmpeg defaults
(including x264/x265). It is retained as historical evidence, not the current
distribution candidate. Android already disabled these options; APK v23 and
car bundle r6 are unchanged. Physical RP5 remains v19/r5 and was not accessed.

This is a specific component-configuration/notices correction, not a complete
legal or whole-game approval. See [FFmpeg's own licensing information](https://ffmpeg.org/legal.html).

Changes: `nix/ffmpeg.nix`, `default.nix`, `shell.nix`, native CMake license test
and notice installation, `tools/FFmpegLicenseCheck.cpp`, `docs/THIRD-PARTY-LINUX.md`
and five actual component license texts in `docs/licenses/`. Lua's notice is
from the linked 5.4.7 header; SDL/MojoShader/Speex notices match actual sources.
Seven notice/license files are present in the installed Nix package.

## Evidence

- Both actual Nix build and refreshed development build pass **10/10 CTest**.
  Regression fails against all four former GPLv3 libraries and passes against
  the actual new LGPL-2.1-or-later libraries, keeping WMA/MP3/PCM decoders and
  ASF/MP3/WAV inputs. Complete actual configuration strings and library hashes
  are in `lgpl-media/ffmpeg-before.json` and `ffmpeg-after.json`.
- Original `audio/music/music.dsb`, start 10 s / duration 0.8 s, through the
  actual Stream decoder and SDL mixer: **38,400 aligned stereo frames exactly
  equal before/after**. Missing input exits 1 and emits no samples. Silent disk
  sink; no audible-mix, whole-bank or long-run sync certification.
- Two character clip requests (one original Biggi clip plus its fallback)
  reach frame 30, with actual Radeon readbacks pixel-identical to the earlier
  timed captures. Audio nonzero; muted negative entirely zero. The Bink decoder
  is OpenJuice's own, not FFmpeg's video decoder.
- Actual r9 store ELF runs **900 frames**, six Ferraris with installed mods,
  driving/lap-1 telemetry, reviewed screenshot and exit 0. Initial validator
  expected a literal `Go` log which is not emitted; exact terminal receipt and
  lap/frame telemetry were verified without rerunning or relabeling a crash.
- All **307 closure paths** import into a fresh owned test store, matching
  dependency graph/content; isolated profile installer and help pass. Runtime
  sample uses the normal Nix store, not the private import root.
- **907 critical source files** plus six notice files match the actual compiled
  Nix source. Engine companion: **1,331 files**, independently restored with
  identical source NAR and derivation. Media companion: **10,147 files**, raw
  upstream NAR identity, both exact applied patches, full configuration and Nix
  build inputs retained. Neither restoration is a second compiler run.
- Current full integration: **260 passed / 1 failed**, the same strict original
  119-D8M inventory failure; no parser bypass or exemption added.
- Six personal Linux save files and installed Wave 2 files retain their hashes.
  No RP5 interaction, APK replacement, commit, push or public upload this pass.
- Owned raw package stage and private import root removed after verification:
  **2,210,838,102 logical file bytes**, not a physical-space guarantee. Archives,
  source and receipts retained; real Nix store and user files untouched.

## Current artifacts

| Artifact | SHA-256 |
| --- | --- |
| `OpenJuice-20261030-r9-linux-x86_64-nix.tar.xz` (319698812 bytes) | `2b2d7c768ae53bde435f61cf15912ae87cc3ceb20d02fdbfaf6383a09217031c` |
| `OpenJuice-20261030-r9-nix-source.tar.xz` (5746952 bytes) | `8a9a4f2dc4b317f4bf2a8517e39ef927b9bd9b4347e56d0282333c8dda61f626` |
| `OpenJuice-20261030-r9-ffmpeg-source.tar.xz` (14531276 bytes) | `5d59515c521a0cdae8bcd0f973a7adfd66146e54b316d744394712e9bb3d5af0` |

Current development ELF: `38ad6cfb6f8948defacfd6713a3e4b1f367ac08523543671353db68e7dca4a1e`.
Actual r9 store ELF: `ab040fcd0fad4bbaeef4308dd36bd293749973bf0e84870ca3c4cc3f16f67301`.
The habitual `~/.local/bin/openjuice` still launches `out/linux/juiced`.
Runtime archive contents were all read back, including the unchanged Android
v23/car-r6 hashes and 18-checkpoint parent chain in the local artifact metadata.

Recheck current bindings with:

```sh
python3 out/validation/linux-current-20261029/linux-package-r9/verify_final.py
```

This verifier intentionally binds current artifacts; historical verifiers may
pin older ELF hashes and should not be rewritten to accept unrelated binaries.
Broader campaign/routes/network/art/physical-device and distribution review
remain open in `verification-closure-2026-10-29.md`.
