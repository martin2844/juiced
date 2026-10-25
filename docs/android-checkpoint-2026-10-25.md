# Android integration checkpoint — 2026-10-25

Base: linux-nixos 711018c, OpenJuice 1.02.5; Android platform/launcher initially
adapted from origin/android 4bf7f1b. The current work retains the newer engine and
Linux mod implementation. Original assets were not edited.

## Implemented and verified

- Native ARM64 APK builds and installs on the user's Android 13 Retroid Pocket 5.
- Full Juiced.zip imported through Android's document picker on both the physical
  Retroid and the API 35 x86_64 emulator; validation occurs before activation.
- Six mods imported through a selected shared folder on Retroid and ZIP on emulator.
  Retroid source folder: Documents/OpenJuice/mods. Activation/toggles are transactional.
- Android native instrumentation passes stock (53 cars), modded (54), all four
  Ferrari/wheels combinations, and DDT image dimensions/alpha/colour tests on both.
- Host tests cover failed/cancelled/unsafe import recovery, mod rollback, save backup
  and restore, code-page conversion, controller routing/dead zones and multitouch.
- Physical SDL diagnostics identify Retroid Pocket Controller (2022:3001). Left
  steering and both triggers reached their full expected ranges. The diagnostic
  exited before all other axes/buttons were logged; no full-range claim for them.
- A physical 900-frame six-Ferrari autopilot race completed, with track, cars,
  reflections, HUD and corrected colours. The subsequent manual Focus drive reached
  58 mph in the captured frame. The user confirmed acceleration/braking/steering,
  pause/resume buttons and audio worked, and requested a target of 60 FPS.
- A short SurfaceFlinger sample during the six-Ferrari race contained 126 intervals,
  mean 40.60 ms (~24.6 FPS), median 33.40 ms. This is a baseline, not sustained
  performance across tracks. Optimization is the next task.
- Release APK signature verification and zipalign with 16 KiB page alignment pass;
  packaged native ELF LOAD segments are aligned to 0x4000 or greater.

## Defects found through actual Android testing

The inherited GLES renderer passed 2/4/8 as GLboolean colour mask components.
Canonical booleans fixed the red-only rendering on the emulator; the correction is
also applied to Linux. Both emulator and Retroid now show full-colour races.

With no mod override, an empty path could resolve to a directory. Bionic's directory
seek result reached 9223372036854775807, causing std::bad_alloc during native data
validation. GameFiles now rejects empty/nonregular paths and failed file sizing.
The previously failing full ZIP import and ARM catalogue tests then passed.

The launcher now keeps progress visible and updates its text without rebuilding
and resetting the scrolling view on every copied block. Native game and launcher
validation use separate processes; installation mutation is locked while playing.

## Evidence and remaining work

Local evidence is under out/validation/android/: retroid-instrumentation.log,
retroid-controls.log, retroid-race.log, retroid-race.png, retroid-manual-race.png,
retroid-frame-times.txt, release-package-audit.txt and release-elf-alignment.txt.
These generated outputs and proprietary assets are intentionally not committed.

Remaining: performance optimization toward 60 FPS, final release update on the
Retroid, and current-build career-save/lifecycle checks. The older October 21
validation describes earlier save tests, not proof for the newly integrated build.
No claim of complete career, every track/mod or all Android-device compatibility.
