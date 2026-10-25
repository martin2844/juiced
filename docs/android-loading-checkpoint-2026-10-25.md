# Android loading fixes — 2026-10-25

User report: slow car previews in the dealership/workshop, and missing animated character heads after career races. Target: Retroid Pocket 5, Android 13, ARM64.

## Confirmed causes and changes

The Android GLES renderer dispatched a single snapshot of its work queue once per frame. The loading thread issues synchronous uploads; after an upload completes it queues the next request. Consequently sequential requests waited one frame each. A real GL regression check with 161 sequential buffer/upload/destruction requests reproduced 162 service frames / 2599 ms on the Linux host GPU. The renderer now services follow-up requests within a 4 ms loading slice, using a condition variable. Empty queues return immediately. OpenGL operations remain on the context owner. The same check completed in 2 service frames / 36 ms. These are isolated host measurements, not car-load times on Android and not race FPS measurements.

Character animations are Bink movie clips. The movie player tested existence of Windows-style paths such as `Biggi\BIGGI_ACCEPT_BET1.bik` directly on Android/Linux. The clip existed but was treated as missing before the decoder was called. Movie selection now uses the existing native path resolver for both initial and numbered fallback clips, including case differences. A real-data integration check failed before the change and now opens, decodes, uploads and draws the normal and lowercase/fallback names through the Android renderer. A truly missing clip still reports missing. This verifies the failure independently of car loading.

## Validation

- Loading queue regression: failure before, pass after; empty queue does not incur the loading budget.
- Existing deferred GPU readback check passed.
- Four host CTest suites passed (race cleanup, touch, touch bindings, code pages).
- Real game character clip normal/fallback/missing checks passed.
- Signed release APK built and verified (APK signature v2); version 1.02.5-android.4, code 5.
- Updated the connected RP5 with `adb install -r`; existing game installation, six enabled mods and profile loaded.
- Career dealership was exercised on the device; car previews appeared and selection changes rendered. Screen recordings and captures are in local `out/validation/android/`.
- No new 60 FPS claim. The loading worker remains serial; these fixes remove synchronization waits and invalid paths rather than increasing memory limits or adding unsafe GL worker threads.

Local evidence: `loading-queue-before.log`, `loading-queue-after.log`, `movie-before.log`, `movie-after.log`, `loading-readback.log`, `loading-ctest.log`, `openjuice-dealer-before.mp4`, `openjuice-dealer-after.mp4`, `loading-dealer-after.png` under `out/validation/android/`. Full animated-head playback at the original reported career interaction still needs user confirmation on the RP5; the decoder/render path was verified with real clips on the host.

## Release publication

The repository remains private. README and repository metadata credit Sidewinder and link to upstream OpenJuice. Android `android-v1.02.5-3` and Linux `linux-v1.02.5-mods1` were published with binary assets and checksums. Android `.4` supersedes `.3` with these loading fixes; original tags/assets are retained.

The Linux release is an x86_64 Nix closure, not an AppImage; importing unsigned local builds requires a Nix administrator. Four Linux CTest suites and profile activation passed. A fresh administrator import was not possible on this host (sudo password required), and that limitation is disclosed in the release notes.

macOS has no published binary yet: GitHub Actions run 40229479743 was rejected before starting with “The job was not started because an Actions budget is preventing further use.” The main-branch workflow builds, tests, packages and publishes only after success. Enable Actions budget and dispatch `macos-release.yml`, or supply a tested app from a Mac. No visibility or billing settings were changed.
