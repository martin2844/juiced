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
  performance across tracks. This motivated the optimization below.
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

At the initial checkpoint, the remaining work was performance optimization, the
release update and current-build save/lifecycle checks; results are recorded below. The older October 21
validation describes earlier save tests, not proof for the newly integrated build.
No claim of complete career, every track/mod or all Android-device compatibility.

## Performance follow-up

The functional integration above is preserved in commit `2abc4cb`.

Removing repeated framebuffer attachment changes and unused texture-unit setup
alone stayed around 22–28 FPS. The major bottleneck was the exposure histogram's
synchronous GPU readback each frame. Android now queues that small read into two
pixel-pack buffers and checks fences without waiting. Exposure consumes completed
pixels from an earlier frame; screenshots and other readbacks retain synchronous
semantics. Other rendering backends keep their existing synchronous implementation.
No resolution or effect settings were reduced.

The first identical six-Ferrari test after this change measured 58.14 and 60.01 FPS
at 1280×720, and completed 900 frames without logged GL errors. The image is saved
as `retroid-perf2-race.png`; timing samples are in `retroid-perf2.json`.
A real-GL regression (`android_readback_check`) passed BGRA/dimension checks,
replacement of the source target, and coexistence with synchronous reads.

The native activity also terminates its dedicated process after SDL joins its
native thread on a finishing activity. This avoids retaining engine globals and
SDL's finished-main state between launches; the launcher is in a separate process.

The extended physical run completed **7,200 frames**, with the opponents finishing
both laps. Twelve separate SurfaceFlinger samples over roughly one minute yielded
**58.22 FPS pooled**, with sample means **53.20–60.01 FPS** and each sample's median
near 16.67 ms. This is near-60 performance on this track, not a locked-60 guarantee.
Raw summaries: `retroid-perf-long.json`; runtime: `retroid-long-race.log`.
Starting the normal title flow after that run succeeded without force-stopping the
application, verifying the dedicated-process restart fix.

## Release checks

The signed release was installed with `adb install -r`. The imported game and all
mods survived. A new `Player` / `MyCrew` profile was created through the actual
in-game photo/logo/phone flow, reported **Save Successful**, and subsequently
reported **Load Successful** after updating to release. Evidence:
`retroid-save.png`, `retroid-release-load.png`, `retroid-save-files.txt`.

Linux regression after the shared interface change: all four CTest suites passed,
including 190 unit checks. The dedicated Android host GL readback regression also
passed. Package signature and 16 KiB ZIP/ELF alignment checks passed.

The distributable is `out/releases/OpenJuice-1.02.5-android-arm64.apk` (~11 MiB),
with the optional mod ZIP and `SHA256SUMS-android.txt` beside it. It is signed with
the retained local release key, not a Play Store/upload key. Original game assets
are not included. Current-device controls/audio confirmation came from the user;
near-60 timing measurements came from SurfaceFlinger, not visual estimation.

The final release also resumed its existing game process after Home/return, with
the expected automatic pause and successful Continue. A single native activity is
retained per task to avoid a second engine instance. The test instrumentation APK
and emulator were removed/stopped after validation.
