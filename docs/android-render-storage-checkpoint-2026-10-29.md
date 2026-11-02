# Android renderer, imports and car pack — 29 October 2026

Local ARM64 delivery: `out/releases/OpenJuice-render-storage-fixes-arm64.apk`.
Version code 17, version name `1.02.5-android.16-render-fixes`.
SHA256: `634ba96bf86dc27a962d7dc9f4482028ac4085883709f928f46c536d581b612b`.

Use with `out/releases/OpenJuice-Car-Visual-Fixes-r5.zip`, SHA256
`3537ef0d6454b7cdca73d09d4f4d1d92f011cba2d65f2c8bf91df20d280e1373`.
The APK and ZIP are separate: install the APK as an update, then import the ZIP
through Mods. Neither artifact contains player saves. Do not uninstall the app
to update it. This build has not been published or installed on the physical RP5.

## Actual renderer corrections

Android lacked the two fixes independently established on Linux. Constant and
inverse constant blend factors fell through to GL_ONE, render state 193 was
ignored, and the corresponding capability was absent. Buffered XYZRHW vertices
were submitted using an unrelated programmable vertex declaration. This affected
post-processing; disabling the effects was not the correction.

The Android backend now uploads independent ARGB blend constants and submits
transformed vertex buffers with their own screen-space layout, reciprocal W,
diffuse colour and UVs. It retains depth testing, depth writes and start vertex.
Immediate-mode drawing resets the transformed stream uniform. No geometry
readback, physics change or blanket effect removal was added.

The shared GPU regression exercises actual rendering and framebuffer readback.
The original Android backend fails white != zero for constant blending. A
constant-only correction then fails with map::at for the transformed stream.
The final backend passes all four groups on the Android host with Radeon and in
the Android ES emulator. A baseline debug APK reports FAIL in that same emulator;
that native result does not identify which assertion failed. Host readback
regressions also pass. The regression entry exists only in the debug APK.

## Import failure and storage correction

The first normal SAF import of the combined pack failed with No space left on
device. Four abandoned assembled overlays occupied about 1.9 GiB; existing mod
payloads occupied about 492 MiB. Rollback retained the installed packages.

ImportJob now clears only `files/mods/.cache`, after acquiring installation.lock.
It cannot remove an overlay while the game holds that lock. Updating packages
copies the incoming payload once rather than copying outgoing payloads first.
Unreplaced packages, collection preferences and both user-enabled and
user-disabled choices survive. An unreadable existing package directory stops
the update before validation/activation instead of treating it as empty.

The game activity also clears abandoned overlays after acquiring that same lock,
before starting the engine. Instrumentation deliberately kills its game process
at completion; repeating it reproduced another 3 GiB of abandoned overlays and
blocked an APK update. The actual Settings -> Check game files action reclaimed
that space and validated 64 cars. The final activity-start check seeds a killed
stage marker and verifies that startup removes it before the native regression.

Host tests cover permission failures, both toggle states, unreplaced packages,
failed validation rollback, interruption recovery, ZIP traversal, save backup
round trips and an actual held FileLock. Independent former-code variants fail
the unreadable outgoing-payload and unreadable package-list checks. Initial
compile-only failures are retained separately and are not negative controls.

After the correction, the real launcher -> Mods -> Add mods from ZIP -> Android
file picker import succeeds in the owned emulator: 64 cars validated. Installed
Ferrari, Street Cars and Wave 2 package payload hashes match the r5 receipt;
free space increased to about 3.2 GiB. This is emulator evidence, not RP5 evidence.

## Packaging and persistence evidence

Release signature and 16 KiB ZIP alignment checks pass. Independent ELF parsing
checks all seven ARM64 libraries, load-segment alignment/congruence and DT_NEEDED
closure against packaged libraries and actual NDK API 26 system stubs. The
release excludes TestLaunchActivity and the native regression entry. These are
distribution checks, not an ARM64 hardware runtime test.

Full Android-host build and CTest pass 4/4. Linux's rebuilt GPU diagnostic still
passes on Radeon. Host Java storage tests pass. Real Android MotionEvent tests
cover multitouch, partial release, cancellation and menu transitions.

The emulator initially had no career profile. A byte-checked copy of Linux's
PTony profile was placed only in the previously absent emulator PPlayer folder.
The save-editor instrumentation uses a further disposable clone to verify
money/car/respect changes, backup, stale-edit rejection and restore. Its original
clone and the Linux source retain their SHA256 values after APK update, normal
mod import and the editor test. This does not constitute checking Ronnie on RP5.

On the final debug APK, the save-editor check passes again. Actual native
catalogue checks pass for the documented pack prefixes (53 original cars, 54
with Ferrari, 57 with Street Cars, 64 with Wave 2), each with wheels enabled and
disabled. A missing prerequisite is rejected. These packs already document
consecutive stable IDs and dependencies; an outdated test had incorrectly
expected a dependent pack without Ferrari to work. The launcher already rolls
back an invalid toggle. Production dependency/ID behavior was not changed.
The updated instrumentation restores every original toggle in finally. Native
image import additionally verifies DDT dimensions/length, padding transparency
and colour channels.

The old touch-race test waited a fixed 20 seconds and captured the fly-by. Its
PASS proves overlay injection only. The strengthened test waits for actual Go
and checks newly written applied CarBody telemetry at every input stage. The
earlier capture is retained and excluded from driving proof.

The strengthened live race passes all six stages: neutral; gas (19.8 m/s,
throttle 1); gas + right (steer 0.86); right released with gas still 1; brake
(speed 0, throttle 0, brake 1); all released (steer/throttle/brake 0). Actual HUD
captures show the held buttons and the car after Go. It injects Android touch
events through the normal -i game path, without --test or autopilot. It is a
control-response check, not a full manual lap or a performance result.

The exercised x86 APK predates the final Java-only storage guards; all native
libraries are byte-identical to the final x86 build. Its APK hash and library
hashes are recorded in `live-driving-receipt.json`. Final APK startup/render
and killed-stage checks have a separate `final-apk-emulator-receipt.json`.
One earlier attempt lost its ADB/UiAutomation connection; that failure is retained
and excluded. The passing repeat uses a dedicated owned ADB server.

Evidence root: `out/validation/linux-current-20261029/android-current`.
Key logs: `render-{before,constant-only,final-host}.log`,
`render-final-emulator-result.txt`, `full-host-build-ctest.log`,
`storage-final-guarded.log`, `storage-negative-original.log`,
`storage-guard-negative.log`, `imported-mods-receipt.json`,
`emulator-save-preservation.json`, `save-editor-instrumented-retry.log`,
`save-editor-final-apk.log`, `catalogues-final-apk.log`,
`touch-driving-live-retry.log`, `live-driving-receipt.json`,
`final-apk-emulator-receipt.json`,
`signature-final.log`, `zipalign-final.log`, `release-elf-receipt.json`.

## Car-vinyl sweep and remaining limits

The native Radeon gallery covers both sides of every catalogue decal on the
retail Focus and all eleven added cars: 774 captures, all 49 contact sheets
visually inspected. Each added car has 31 retail decals; Ferrari also has three
specific slots. Ferrari's blank custom-logo receiver is intentionally blank
without an imported image. No new side-UV failure was found at these fixed
viewpoints. The gallery is `all-vinyl-gallery/index.html`; its review receipt
binds every contact sheet and the capture executable by SHA256.

This sweep uses stock body kits. Earlier eight-angle galleries additionally
cover stock/carbon/vinyl/maximum-kit states, but neither set certifies every
kit/hood/damage combination. The source models still vary in artistic detail,
especially the Outback. Arbitrary imported logos, physical RP5 rendering and
driving, 60 FPS, full campaign/network coverage and the strict original-model
inventory failure remain outside these passing checks. See
`verification-closure-2026-10-29.md`; the full project is not declared complete.

The local backup `out/releases/OpenJuice-source-checkpoint-20261029-render-storage.zip`
contains the tracked binary patch against its recorded HEAD and all untracked
source files, with hashes. It does not stage/reset/commit existing work, embed
game data or replace a final clean release commit. Its independent ZIP readback
and reverse-patch check are recorded in `worktree-checkpoint-receipt.json`.
