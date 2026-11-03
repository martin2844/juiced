# Native effects: Android and Mac validation — 3 November 2026

This follows `4daca5bbf4d832547602edd0d0c77fae8908bed1` and supersedes the
earlier native-effects checkpoint's interpretation of the black race captures.
The user requested an Android build and device test, followed by the Mac fixes.

## What the black captures actually showed

Juiced stores lighting/exposure information in the framebuffer's alpha channel.
The native screenshot functions exported that alpha as image transparency.
Resizing those RGBA images premultiplied RGB by the effects mask, making intact
roads, buildings and car surfaces appear black or speckled. The apparent defect
also appeared in screenshots from the unchanged Linux production executable.

Evidence that distinguishes an image-export problem from a rendering problem:

* Convert the **original full-size** BMP to RGB **before** resizing it. Roads,
  foliage and cars are intact in both the old Linux and Metal captures. Converting
  an already resized RGBA preview cannot recover discarded RGB.
* A direct Android `adb exec-out screencap -p` of the running RP5 shows intact
  surfaces, independently of OpenJuice's screenshot exporter.
* The new `opaque-screenshot` GPU regression clears the backbuffer with RGB
  `(160,64,32)` and alpha zero. The previous exporter fails its opaque-image
  assertion. The corrected exporter passes and preserves the raw GPU alpha zero.

`capture()` now interprets its readback as `SDL_PIXELFORMAT_BGRX32` on Linux,
Android and Metal. This exports an opaque screen image while preserving RGB.
It changes neither live rendering nor the effects' alpha buffer. `readTarget()`
and diagnostic `captureTarget()` deliberately retain raw render-target alpha.

The earlier generic blend/output/NaN experiments did not diagnose this export
issue. Their temporary instrumentation is absent from the resulting source.
The independent shader/state corrections that have focused failing GPU tests
remain; they are not claimed as the explanation for the black image previews.

## Additional real shader correction

The legacy ps_1_1 `TEXCOORD` instruction saturates XYZ and forces W to one.
The previous MOV_SAT lowering preserved the interpolator's W. GLSL and Metal
now supply W=1 before the lowered instruction. The `texcoord-alpha` GPU case
passes input W=0 and verifies both the output alpha and source-alpha blending.
The old translator fails this case; corrected GLSL and Metal pass it.

Primary instruction semantics:
<https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/texcoord---ps>.

## Android build, installation and save protection

Device: Retroid Pocket 5, serial `7f3ffc8d`, Android 13, Adreno 650.

* The pre-existing career was saved and exited through its own menu before any
  update. The launcher's save-export UI produced
  `RP5-before-native28-20261103.zip`; a copy exists off-device.
* The installed Android 19 APK was copied off-device for rollback.
* All APK installations used `adb install -r` with the existing release signing
  key. There was no uninstall, data clear, roster rewrite or career editor use.
* Debug APKs temporarily allowed native GPU tests and isolated smoke races.
  Their fixed `files/validation` support directory links game/mod payloads while
  keeping their settings, saves and logs separate from the actual career.
  The test-only forwarding activity is absent from the release APK.
* The 405 smoke race rendered 3,600 frames with six cars and positive speed;
  a direct physical-screen PNG was inspected. This used WorldLight, Sky,
  Headlights, SMAA, AO and heat-haze settings in the isolated test root.
* All eight exported `ModSaves` files matched their pre-update SHA256 values
  after the debug tests and mod import, including both profile archives.
  A second backup exported through the release launcher's UI after the practice
  run (`RP5-after-native28-20261103.zip`) also matches all eight hashes exactly.
* The final **release** APK was installed and launched from the usual launcher.
  Its Quick race/practice scene and pause/abandon controls were exercised and
  captured through Android's own screen capture.

APK: `1.02.7-android.28-native-effects`, versionCode 28, arm64-v8a.
SHA256:
`6459b795a6606785fd5504c51ec5614c7fe7ae8a9fad82d1ae06ecf58c4559eb`.
The installed package has no DEBUGGABLE flag.
The launcher's About panel now reads the installed package version instead of
showing the hardcoded `1.02.5` string.

### Current cars and liveries

Imported through the launcher's normal **Add mods from ZIP** workflow:

* Street Cars Wave 2 revision **21**, including the accepted 405 refinements.
* AI Hero Liveries revision **2**, registered for all installed cars.

Existing other packs and enabled/disabled choices were preserved. The launcher
reported **64 catalogue entries validated**; this includes its sentinel entry,
not 64 playable vehicles. The collection has 63 playable cars: 52 stock plus
Ferrari, three first-wave Street Cars and seven Wave 2 cars. Import validation
is not a claim that every kit on every vehicle has been visually re-reviewed.

## Verification

* Linux unit suite: **738 passed, 0 failed** (`juiced_tests --unit`).
* Linux native GPU checks: **18 passed**, including both new negative-control
  regressions. The original exporter fails `opaque-screenshot` as expected.
* Physical RP5 native GPU checks: overall **PASS** after all 18 cases; the
  pulled archive contains the new opaque backbuffer BMP, whose decoded RGB
  is constant `(160,64,32)` with no image alpha channel.
* ARM64 debug and signed release Gradle builds: successful.
* Mac earlier native run: **730 unit assertions passed**, then all **17** GPU
  cases including the new TEXCOORD test passed before the Mac disconnected.
  RGB inspection of the earlier Metal day/night races and imported-405 gallery
  disproves the black-surface blocker reported in the earlier checkpoint.

The broad unit runner was initially invoked without `--unit` or game/save fixture
arguments and correctly reported two missing integration fixtures. The required
unit invocation above passes. That fixture error is not a renderer regression.

No 60-FPS guarantee, full game playthrough, arbitrary Android GPU support or MSAA
support is claimed. The original Linux launcher executable remains unchanged:
SHA256 `295d980d1d0079c19a978774f94c619b739e073415a1f6e85eb172c34fc0bcdf`.

## Mac completion status

The isolated checkout is
`/Users/martin/Projects/juiced/openjuice-effects-20261103`; game assets are
`/Users/martin/JuicedData`. Native Metal, shaders, mods, CLI and app targets built
there in the preceding pass. The TEXCOORD correction compiled and passed its
GPU test on the Mac during this follow-up.

The Mac subsequently went offline at Tailscale address `100.98.44.120`.
The final BGRX screenshot-export correction is ready in local source, but its
last Mac rebuild/test still requires reconnection. The normal installed Mac app
and user support/save directory have not been replaced. Do not describe that
last rebuild as completed until its log has been obtained.

## Evidence and reproduction

Host evidence directory:
`/run/media/martin/Ediloco/OpenJuice-builds/20261103-android-then-mac`.
Contains the signed APK, old APK, save backup and per-file hash manifests,
actual RP5 screenshots, GPU captures, negative-control logs and build logs.
`rp5-before-release.tar` contains an isolated-race log/capture and read-only
copies of the user's saves. Keep it private; it is not a release asset.

Build:

```sh
out/android-shell/bin/openjuice-android-shell tools/build-android.sh release arm64-v8a
nix-shell shell.nix --run 'cmake --build out/native-effects --target juiced juiced_native_effects_check juiced_tests -j 6'
out/native-effects/juiced_tests --unit
SDL_VIDEODRIVER=wayland LD_LIBRARY_PATH=/run/opengl-driver/lib \
  out/native-effects/juiced_native_effects_check /tmp/oj-native-gpu
```

On the Mac, sync the changed source into the isolated checkout, then build
`juiced juiced_tests juiced_native_effects_check juiced_app`, run the unit suite
with `--unit` and the GPU executable, and inspect a fresh opaque day/night race
capture. Keep Mac user saves isolated using `JUICED_SUPPORT_DIR`.
