# Android crew intensity controls — 2026-10-28

The Android SDL adapter previously folded the D-pad into steering and menu navigation, but provided no controller bindings for the six DRIVER_TACTICS actions. The existing race HUD already implements the intensity changes.

## Mapping

During driving, the left stick steers and the D-pad controls crew intensity:

| Input | Action |
| --- | --- |
| Up / Down | First teammate: increase / decrease |
| Right / Left | Second teammate: increase / decrease |
| Select (SDL Back) + Up / Down | Third teammate: increase / decrease |

Each press changes one level, clamped by the existing HUD to Low, Medium, High. The third-teammate modifier suppresses the ordinary teammate commands. Menu D-pad navigation remains unchanged. This uses console-style D-pad commands; it is not a claim of an exact historical PS2 mapping.

Android adds six independent one-shot bindings after loading the retail maps and after saving keyboard settings. Reapplying is idempotent. Keyboard and touch bindings remain intact. The new semantic names are registered in the shared parser, but the bindings and SDL handling are Android-only. The launcher Controls tab explains the mapping.

## Verification

- Android host controller integration passed using the actual retail controls file and an SDL virtual gamepad: all six actions isolated, no steering side effects, no repeated commands while held, menu selection/back/navigation, trigger independence, deadzone, remapping, unplugging and keyboard fallback.
- Existing RaceHudOrders test: 2 checks passed, covering actual intensity mutation and opening the orders panel.
- Signed ARM64 release APK built successfully: versionCode 14, versionName `1.02.5-android.13-crew-controls`.
- Artifact: `out/releases/OpenJuice-crew-controls-arm64.apk` and adjacent SHA-256 file.
- Controller log: `out/crew-controls-validation/controller.log`.
- No installation performed: connected device was an iPlay 70 mini Pro, not the RP5. Actual RP5 race input remains unverified.
- No saves changed. This APK also includes the prior uncommitted poker/card/preview/menu fixes; it is not an isolated production release of the input change.
