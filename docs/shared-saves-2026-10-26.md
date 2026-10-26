# Shared careers across all mod configurations

User policy: adding, removing or disabling mods must not hide or isolate saves.
Potential mod incompatibilities are accepted; no catalogue compatibility gate
or forced new career is wanted.

Android .10 (versionCode 11) implements one career directory, `ModSaves`, for
both original-game and modded launches and both save-editor catalogue modes.
Linux and macOS entry points use the same shared-directory policy. Explicit
desktop save-path overrides remain supported. The old package-combination
selector no longer changes paths or consults `collection.ini` for saves.

Legacy careers in `saves`, `LegacyModSaves` and `ModSaves/variants/*` are copied
into the shared directory. Current profiles and originals are never overwritten
or deleted. Folder collisions preserve both copies with an import suffix.
Receipts avoid repeated imports and resurrecting a deliberately deleted profile;
a stored destination lets an interrupted import resume. Creative settings and
records are copied only where the shared directory has no existing file.

No car/part index remapping or compatibility analysis is performed. Existing
save loading still interprets indices using the current catalogue. Mod changes
may change a fitted part or cause loading/runtime failure; that is not prevented
by hiding the career in another directory.

Validation:

- Package integration test passes all four Ferrari/wheels combinations, all mods
  disabled, and custom-logo toggling with a constant save directory.
- Recovery tests cover conflicting profile folders, intact originals/current
  progress, creative settings, repeated launches and deleted imported profiles.
- Signed Android ARM64 release build succeeds and updates the Retroid in place.
- Retroid save editor shows Player and Ronnie in both original-data and
  enabled-mods modes with the curated vinyl collection still enabled (8/8 mods).
  No edit/apply operation was used.

Evidence: `out/shared-saves-host-validation.log`, Android build log, and
`out/retroid-vinyls-20261026/shared-saves-{stock,modded}.png` (local/private).
The earlier exported backup is preserved. macOS build remains for the local Mac.

APK: `out/releases/android-shared-saves/OpenJuice-1.02.5-android.10-shared-saves-arm64.apk`

SHA256: `6100182852fbf8ebff0eb49ce8a58db451043adb29400368ebe14544929dd054`

Linux native executable also compiles successfully (`out/shared-saves-linux-build.log`).
Android reaches the title/attract screen with all eight mods enabled. Profile
visibility was verified in the save editor in both modes; a career gameplay
session was not exercised in this check. Device left at the launcher.
