# Android port validation

Target: Retroid Pocket 5, Android 13, ARM64. Local validation date: 2026-10-21.

## Completed checks

- Android ARM64 C++ build and Gradle debug APK packaging succeeded with NDK r28c and JDK 17. Native game code is optimized (`-O2`) even in the debuggable APK.
- All seven packaged shared libraries have ELF load-segment alignment of at least 16 KiB. Every non-system dependency is present in the APK. `zipalign -c -P 16 4` passed; `apksigner verify` passed (v2 signature).
- All 137 extracted retail shader programs compiled both under desktop OpenGL and **OpenGL ES 3.0 through ANGLE**. The latter uses the actual GLES shader text, without desktop GLSL substitution.
- Real retail data loaded 53 car definitions through the Android core configuration.
- A 2,100-frame host race reached the second lap under autopilot. A further 1,800-frame run exercised the capped offscreen framebuffer and produced no reported OpenGL frame errors. Screenshots showed the track, car, reflections, minimap, speedometer, and lap HUD.
- SDL virtual gamepad integration with the actual retail controls file passed A/select, B/back, D-pad navigation, independent RT/throttle and LT/brake, steering, trigger clearing on disconnect, and keyboard fallback.
- Touch tests passed simultaneous fingers, shared bindings, movement between regions, release, focus reset, and out-of-bounds rejection.
- Java importer tests passed wrapped and flat archives, executable filtering, path traversal rejection, preservation of existing data after failure, replacement, interrupted-swap recovery, and mixed-case retail filenames.

## Android runtime validation

The final APK installs and updates successfully on the Android 13 / API 33 ARM64 emulator. The imported 1.9 GiB dataset survives `adb install -r`.

- The Android Storage Access Framework picker imported a wrapped test ZIP, activated its data folder, and omitted its Windows executable. Full retail game data was then streamed into app-private storage through ADB to avoid storing a second full ZIP inside the emulator.
- Native diagnostics created an OpenGL ES 3 context and loaded all 53 car definitions from that data on Android's case-sensitive filesystem.
- Instant race loaded the real track, models, shaders, and audio banks. The countdown completed and touch acceleration drove the car to 68 mph; screenshots show the car, scenery, reflections, and HUD. No shader/program failures or OpenGL frame errors were reported during the run.
- Android Back opened the in-game pause menu; selecting Continue resumed play. Home/return restored the running game without a crash.
- Android AudioFlinger recorded the app's active 48 kHz stereo float stream with millions of frames written and nonzero signal power. The track became inactive while backgrounded and active again after return. This verifies decoding/mixing/output in Android, not Retroid speaker quality or latency.
- The final build navigated the title screen, profile manager, on-screen keyboard, crew logo, and phone selection. Creating driver `Player` / crew `MyCrew` produced **Save Successful** and wrote the profile under app-private `files/saves/SAVE` plus `reg.cf1`.
- Closing native diagnostics and launching the game again worked. SDL's default process restart on a subsequent native launch avoids retaining an old native game instance.
- Controller selection was corrected to prefer an analog gamepad over Android's D-pad-only keyboards. Its regression test passes alongside the retail binding tests.

**Not measured:** Retroid frame rate, Adreno shader-driver behavior, thermal stability, audio latency, or the exact built-in controller mapping. No Retroid was connected through ADB during this validation. Those require a physical handheld run.

## Remaining limitations

Intro movies are skipped. Legacy online services, voice chat, steering-wheel force feedback, and gamepad rumble are unavailable. This is a development APK, signed with the local debug key. Rendering approximates some legacy fixed-function effects; it is not a claim of perfect original-game parity.
