# Curated vinyls: Android installation and publication

Published prerelease: https://github.com/martin2844/juiced/releases/tag/vinyls-v3-curated

The release contains the compatible Android .9 ARM64 APK and the corrected
`OpenJuice-Underground-Vinyls-v3-curated-r1.zip` (107 A/B options).
The initially uploaded ZIP was withdrawn: its `version=3` field was mistakenly
used as a collection revision. The Android importer correctly rejected it.
The corrected manifest uses `version=1`, `revision=3`. Both generators now follow
that schema, and packaging rejects unsupported manifests before creating output.
The correction is in commit `ccd94be` on `linux-nixos`; the release tag identifies
the earlier collection checkpoint and the notes explicitly link the correction.

Corrected ZIP SHA256:
`77dcaf62468fcd7ea7f8fb35fccfa82caee8667c28b7acf8e18ff1436f1f1776`.
Verified against the device copy and GitHub release asset digest.

Retroid Pocket 5:

- Updated installed .8 to .9 with `adb install -r`; no uninstall or data clearing.
- Exported saves through the launcher before importing the mod. The ZIP passed
  integrity verification (9 entries), with a local copy in ignored
  `out/retroid-vinyls-20261026/OpenJuice-before-vinyls-20261026.zip`.
- Imported r1 through the actual Android file picker and ModStore transaction.
- Confirmed the collection shows Enabled; all seven pre-existing mods remain
  enabled (8/8 total).
- Ran Check active mods after activation: `OK: 54 cars loaded. Data and active
  mods validated.` Screenshots saved in ignored `out/retroid-vinyls-20261026/`.

This validates installation and the active native catalogue on device, not
individual on-device renders or races for every design. The previous host test
covers loading all 107 options in garage/race and save roundtrip.

Enabling this asset mod selects a different career catalogue variant. A blank
profile list with it enabled is not deletion: disable only this collection to
return to the previous mod configuration and its careers. No save migration was
performed. The backup is private and is not included in the GitHub release.
