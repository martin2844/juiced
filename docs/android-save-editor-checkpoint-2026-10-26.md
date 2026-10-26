# Android save editor — 2026-10-26

Version: 1.02.5-android.5, versionCode 6, ARM64.

## Implemented

- Launcher Saves section and native save editor: balance, additional stock cars,
  and respect for eight rival crews. Review required before applying.
- Original and currently enabled-mod profile roots resolved by the same native
  package code as the game. Model names are validated against the active roster.
- Uses the engine's Archive and Career readers/writers, recalculates archive hash,
  preserves other archive entries and verifies the serialized result can be read.
  Unsupported career layouts are rejected by exact read/write roundtrip.
- Money range 0–999,999,999; new respect values 0–1,500 (existing out-of-range
  values can remain untouched); at most 55 cars. Added cars have initialized damage
  grids and garage padding. Existing cars and race history remain intact.
- Installation lock excludes the game and import jobs. A stale snapshot cannot
  overwrite a newer save. Android AtomicFile replaces the archive after the prior
  bytes and target path have been synced to a unique recovery directory.
- Restore also backs up the current save, so it is reversible.
- Original yellow J icon from src/JuicedConfig/res/juiced.ico, identical rendered
  PNG to the installed game's game.ico. Credit retained in assets/licenses.
- Career reader rejects impossible car counts before allocating memory.

## Verification

Host `android_save_editor_check` passed against the original 53-car catalogue and
54-car mod catalogue: money, all crews, car model/grid/padding, unknown archive
entry preservation, and rejection of negative money, excess respect, unknown
models, garage overflow and a corrupted archive hash.

Connected Retroid Pocket 5, Android 13: `save-editor` instrumentation passed using
a temporary clone of the existing PPlayer archive. It edited balance to 123456,
set all eight respect values to 1200, added a car, checked persisted bytes and
automatic backup, rejected a stale edit, and restored the original bytes exactly.
The test asserted the real PPlayer archive was unchanged and removed its clone
and test backups. No example edits were applied to the real player profile.

Evidence under ignored local out/validation/android:

- save-editor-stock-test.log, save-editor-mods-test.log
- save-editor-device-test.log
- save-editor-test-build.log, save-editor-release-build.log
- pre-save-editor-backup.tar (private real saves; never commit or publish)

## Limits

This is manual recovery of values, not automatic reconstruction of a crashed
race's rewards. It does not fabricate wins or host-challenge completion. Normal
gameplay may clamp respect to earned minimums. Existing engine progression logic
remains responsible for respect-triggered events. Engine serialization and storage
were tested; a full career playthrough of every added vehicle was not performed.
Editor backup history remains internal; Settings exports active saves only.
