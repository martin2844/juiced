# Android / Retroid Pocket 5

This port builds OpenJuice 1.02.5 as a native Android application, with SDL3 input,
OpenGL ES graphics, FFmpeg audio and the same engine/mod code as the Linux port.
It requires Android 8 or newer, ARM64 and OpenGL ES 3. Original game data is not
included in the APK.

## Installation

1. Install the signed ARM64 APK. Open OpenJuice and choose **Choose juiced.zip**.
2. Select your ZIP containing the PC game data. A single wrapping folder is
   supported. Keep at least 3 GiB free for the first import; replacing an existing
   installation needs room for both copies. The importer filters Windows binaries,
   rejects unsafe paths and validates the native game catalogue before activation.
3. Optionally open **Mods** and import a package ZIP or choose a folder containing
   the mod folders. On the tested Retroid the folder is
   `Documents/OpenJuice/mods`. Android grants access through its system picker.
4. Choose **Start playing** for title/profile/career menus or **Quick race**.

A collection has `controls/mod.ini`, `ferrari/mod.ini`, `wheels/mod.ini`, etc.
Each package also contains its assets and/or `main.lua`. The six bundled packages
are controls, custom-logo, driver-photo, Ferrari, neon and wheels. Validation loads
active scripts and game definitions; it cannot prove every visual asset is correct.

The selected shared folder is an import source. After editing it, press **Refresh
linked folder**. The application uses an internal validated copy while playing;
it does not modify the source folder. On/off choices are retained when refreshing.
Invalid imports preserve the previously active installation. The game process holds
an installation lock while it runs.

## Controls and saves

The left stick steers, RT accelerates, LT brakes, D-pad navigates, the south face
button selects, the east button backs out, and Start pauses. **Controls** offers
**Set up gamepad**, with remapping, stick dead-zone adjustment and a live input monitor.
**Set up touch controls** provides a draggable driving layout with per-button actions,
individual driving/menu button sizes (50–300%) and opacity. Steering, gas and brake
default to 145%, gears to 140%, and secondary driving buttons to 80–90%.
Save applies the layout. The Controls-tab switch disables touch immediately;
enabling it selects Auto. Auto shows it when no analog gamepad
is connected; Always also shows it on handhelds; Off hides it. Menus and pause
automatically use a navigation pad with Select, Back, Tab, page keys and the paint
purchase shortcut. Driving buttons resolve against the active keyboard map, including
stock, Modern Controls and saved keyboard bindings. Multiple fingers can steer and
accelerate together; cancellation, pause and backgrounding release held inputs. The Retroid's GAS/BRAKE
Android axes are supported in the Java monitor; SDL normalizes the game input.

Game data, controls and saves live in app-private storage. Stock and modded careers
use separate save roots. **Settings → Export save backup** writes a backup ZIP through
the system picker; **Restore save backup** validates and transactionally replaces saves.
Updating an APK with the same signing key preserves this data. Uninstalling Android
applications removes their private data, so export saves before uninstalling.

Photos and logos use the Android image picker. JPEG/PNG inputs are oriented,
scaled/cropped or padded, and converted to the engine's DDT format locally.

## Build on NixOS

Build the FHS environment with `nix-build android-shell.nix -o out/android-shell`.
Install SDK platform 35, build-tools 35.0.0, NDK 28.2.13676358 and CMake 3.31.6 in
`out/android-sdk` (or set `ANDROID_HOME` to an existing SDK). Gradle uses Java 17.

```sh
out/android-shell/bin/openjuice-android-shell tools/build-android.sh debug arm64-v8a
out/android-shell/bin/openjuice-android-shell tools/build-android.sh release arm64-v8a
```

Release signing requires `OJ_KEYSTORE`, `OJ_STORE_PASSWORD`, `OJ_KEY_PASSWORD`
and key alias `openjuice`. The build script optionally reads these variables from
`~/.local/share/openjuice/android-signing.env`. Keep the key and password file out
of the repository. Local debug builds use the same key when configured, allowing
updates to a release without losing imported data.

For emulator validation use `debug arm64-v8a,x86_64`; `test` builds the separate
instrumentation APK. Debug-only `TestLaunchActivity` exposes deterministic smoke
runs and is absent from release builds.

## Verification and limits

See [the individual button sizes checkpoint](android-button-sizes-2026-10-25.md)
for version `1.02.5-android.3`. See [the UI and touch checkpoint](android-ui-touch-checkpoint-2026-10-25.md) for
version `1.02.5-android.2` and its screenshots. See [the 2026-10-25 checkpoint](android-checkpoint-2026-10-25.md) for the actual
Retroid results. The older [Android validation](ANDROID_VALIDATION.md) describes
prior work from October 21; its measurements are not results for this build.

Intro video playback, legacy online/voice services and force-feedback/rumble are
not validated or supported by this Android release. Rendering retains approximations
of legacy graphics effects. Compatibility with arbitrary third-party mods and all
Android GPU drivers is not guaranteed by the catalogue validator.
