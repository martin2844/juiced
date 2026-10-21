# Juiced for Android

## Platforms and downloads

This repository keeps the native ports on separate branches: [macOS (`main`)](https://github.com/martin2844/juiced/tree/main) and [Android (`android`)](https://github.com/martin2844/juiced/tree/android). Each branch contains its full engine source and platform build instructions. Platform fixes are not automatically shared between branches.

Download the Android development APK from [Releases](https://github.com/martin2844/juiced/releases). On first launch, choose **Import Juiced ZIP** and select your own game archive; game data is not included.

Native ARM64 Android port of [OpenJuice](https://gitlab.com/sidecr3w/openjuice), derived from the working macOS port. Targets the **Retroid Pocket 5** and uses SDL 3, OpenGL ES 3, and FFmpeg audio decoding. No Windows emulator, Wine, or game assets are included.

## Install and play

1. Copy the APK and your own `juiced.zip` to the handheld.
2. Open the APK in Android's file manager and allow installation from that app if prompted.
3. Open **Juiced Android Dev**, choose **Import Juiced ZIP**, and select the archive. Leave about 3 GB free for the imported data; replacing an existing installation temporarily needs another copy's worth of space.
4. Choose **Play Juiced** for the menus or **Instant race** to start driving. **Check controls and graphics** tests gamepad input and data loading.

The archive may contain the game files directly or inside one folder. It must contain `tracks/`, `carmodels.dat`, and `scripts.dat`. Windows executables and DLLs are omitted. Imported files and saves remain in the app's private storage. Uninstalling the app removes them; installing an update over it preserves them.

This development APK targets Android 8 or newer, ARM64, and OpenGL ES 3. The Retroid uses its Android/Xbox-style gamepad mode; the button names below refer to their physical positions.

| Control | Action |
| --- | --- |
| Left stick / D-pad | Steer; navigate menus |
| Right trigger | Accelerate |
| Left trigger | Brake |
| South / A | Select; alternate throttle |
| East / B | Back; nitro while racing |
| West / X | Handbrake |
| North / Y | Change camera |
| LB / RB | Shift down / up |
| Start | Pause / select |
| Left stick click | Reverse |
| Right stick click | Rear view |
| Android Back | Back / pause |

If no gamepad is detected at launch, labeled touch regions appear. Gas and brake also navigate up and down in menus. A keyboard's arrows, Enter, and Escape work too. Touch is a basic fallback; physical controls are the intended way to play.

## Build

Requirements: JDK 17, Android SDK platform 35 and build tools 34, NDK **28.2.13676358 (r28c)**, CMake **3.31.6**, Ninja and `make`. The FFmpeg cross-build script supports macOS and Linux hosts. Set `JAVA_HOME` and either `ANDROID_HOME` or `android/local.properties` (`sdk.dir=/absolute/path/to/sdk`).

```sh
cd android
./gradlew :app:assembleDebug
```

The APK is `android/app/build/outputs/apk/debug/app-debug.apk` relative to the repository root. This build is debuggable but uses optimized native code. Keep the same signing key for updates that preserve data. Gradle uses the standard local Android debug key; this is a development distribution, not a Play Store release.

The build downloads pinned SDL and MojoShader sources and verified FFmpeg 7.1.1 source. To reuse local dependencies, pass `-PsdlSource=/path/to/SDL`, `-PmojoSource=/path/to/mojoshader`, and `-PffmpegArchive=/path/to/ffmpeg-7.1.1.tar.xz` to Gradle. Game files are imported at runtime and are never needed to build.

With USB debugging enabled:

```sh
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n org.openjuice.android/.LauncherActivity
adb exec-out run-as org.openjuice.android cat files/Juiced-android.log
```

## Implementation and validation

The GLES backend translates the retail Direct3D shader bytecode with MojoShader, implements render targets, texture decoding, vertex declarations, blending, depth/stencil, and shader registers. Rendering scales to at most 1280×720 internally. FFmpeg shared libraries decode the game's music; the native SDL audio mixer handles playback. Windows-authored file paths are normalized and resolved without case sensitivity on Android.

Host validation uses the same engine, renderer, and platform code, with desktop GLSL syntax for macOS's OpenGL implementation:

```sh
cmake -S . -B out/host -G Ninja -DOJ_ANDROID_HOST_CHECK=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/host -j8
ctest --test-dir out/host --output-on-failure
out/host/android_data_check /path/to/JuicedData
out/host/android_controller_check /path/to/JuicedData
out/host/juiced_android_host --game /path/to/JuicedData -i --autopilot --frames 2100 --capture out/race.bmp
```

Host builds additionally need SDL's platform prerequisites and FFmpeg development packages discoverable through pkg-config. `gles_shader_check` accepts a directory of extracted retail shader bytecode. The importer has standalone Java tests in `tests/Android/GameDataImportTest.java`.

Completed host checks: all 137 extracted retail shaders compiled under desktop GL and OpenGL ES 3 via ANGLE; actual data loaded 53 car definitions; a 2,100-frame race drove into its second lap; retail controller bindings passed menu, steering, trigger, disconnect, and keyboard fallback tests. The APK also passed Android 13 ARM64 emulator checks for installation, import, racing, audio output, pause/resume, and profile saving. **These checks do not establish Retroid frame rate or driver compatibility.** See `docs/ANDROID_VALIDATION.md` for APK/device results.

Intro movies are skipped. Legacy online services, voice chat, steering-wheel force feedback, and gamepad rumble are not implemented. Some legacy fixed-function rendering features are approximated. Saves use private app storage. The upstream noncommercial license applies.

## Provenance and licenses

Upstream: `sidecr3w/openjuice`, branch `retail/1.01/dx9`, commit `bd7e638b187e999c13f85a608d007108a228a05b` (see Git history for the exact upstream revision). macOS base: `3f3ee7f`. This Android repository has independent Git history/storage and does not alter either source checkout.

Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice)

See `LICENSE.md` for PolyForm Noncommercial 1.0.0, `third_party/` for component licenses, and the APK's **Licenses** button. SDL and MojoShader use zlib licenses; FFmpeg is dynamically linked under LGPL 2.1 or later. FFmpeg's exact source URL, checksum, and reproducible configuration are recorded in `cmake/AndroidFFmpeg.cmake` and `tools/configure-android-ffmpeg.sh`.
