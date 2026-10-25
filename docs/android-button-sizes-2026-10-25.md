# Android individual button sizes — 2026-10-25

Version `1.02.5-android.3` (code 4), based on UI checkpoint `241915e`.

- Every driving and menu button has its own 50–300% size. Select a button in the
  appropriate preview, adjust **Selected button size**, and Save.
- Defaults: steering, accelerator and brake 145%; gears 140%; nitro, handbrake,
  pause, select and camera 80%; reverse 90%. Menu directions 140%, Select/Back
  85%, auxiliary buttons 80%. Enlarged circles are kept inside the view; users
  can reposition driving controls to avoid overlaps in highly enlarged layouts.
- Controls now has a touch enable/disable switch. Off hides the overlay regardless
  of controller detection. On chooses Auto, which hides it with an analog gamepad.
  Always remains available in the editor. Returning from the editor refreshes
  the switch and its explanation.
- Legacy global scale is applied once when migrating to independent sizes. Existing
  button positions, action mappings, opacity and visibility are preserved.
- Reset restores the new size hierarchy while retaining visibility mode.

## Validation

Pure Java tests pass for independent driving/menu size persistence, legacy scale
migration without repeated multiplication, visibility/layout/mapping retention,
invalid bounds, default hierarchy and existing multitouch ownership cases.

On the Retroid, used the actual editor to set left steering to 300% while right
steering remained 145%, and menu Select to 238%. Saved, reopened and checked the
persisted independent values. Verified switch Off stored visibility=2 and On
returned to Auto. Restored the default hierarchy after these tests.

ARM64 debug and signed release builds passed. Release signature and 16 KiB ZIP
alignment passed. Updated the Retroid in place, keeping game data, mods and saves,
and left touch disabled as requested. No new performance claim is made.

APK: `out/releases/OpenJuice-1.02.5-android-arm64.apk`

SHA-256: `d8100d0dde783b1857b3b7561acf1b3996944d5b2662efc77ee3a7a1f7ef657a`

Local screenshots and logs: `out/validation/android-button-sizes/`.
