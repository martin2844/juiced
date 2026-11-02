# Post-race respect/history autosave correction — 2026-10-31

The normal Linux launcher now uses the corrected development executable
`6ea148a3c877982a174359f5c98169f541a5b32a98f1a0ccc0f1f26dd9e92b7a`.
This resolves the post-race metadata discrepancy found in the
[collision/repair checkpoint](collision-repair-verification-2026-10-30.md).
No installed car assets, personal saves or physical devices were changed.

## Actual bug and resulting behavior

`CMSRespectChanges` requested autosave while its per-note animation still owned
the serialized crew respect. Canonical `respectNow` was restored only when the
screen subsequently left. `Frontend::afterRaceResults` also recorded the event's
history after that autosave, and before the leave restored canonical respect.
Thus an immediate reload could restore the wrong respect, omit the last history
point, and later record the animated value in that history even after a second
save corrected the live respect.

In the controlled normal-UI reproduction, the untouched r9 executable gives:

| Observation | T.K. respect | History position | Latest event's respect sample |
| --- | ---: | ---: | ---: |
| r9 immediate post-event autosave | 568 | 1 | Event not recorded yet |
| r9 subsequent Career save-and-exit | 584 | 2 | 568 |
| Corrected immediate autosave | 584 | 2 | 584 |
| Corrected save-and-exit / fresh reload | 584 | 2 | 584 |

The correction commits final crew respect and refreshes the final meters before
the summary is accepted. The terminal summary records history before requesting
autosave. It then calls `afterRaceResults(true)` so the continuation does not
record it twice. A meeting summary with further respect notes waits for that
screen; a meeting-only terminal summary records before its own autosave. The
existing no-summary continuation retains its one history step. Save format,
car IDs, prices, physics and individual note animation remain unchanged.

## Runtime and regression evidence

Two actual Linux games take the same owned synthetic Career event through the
normal title/driver/load/calendar/setup/grid/race/pause/abandon/results path,
one with preserved unmodified r9 and one with the fixed development executable.
Page Down invokes the shipped "Skip All" action. Both use zero fee/prize/title
and the previously UI-purchased 406. They are normal last-place abandonments,
not completed laps, wins or naturally earned campaign progress. Input hooks
only inject SDL controls and read rendered pixels; no game state is patched.

For the fixed game, the entire archive from the first results autosave equals
the later Career save-and-exit archive. A third, separate process loads it and
visibly shows 584 in Respect Overview; its entire archive is still identical:
`9c2245fa0b3582acb2b345f3927a4ef205b2cfc815642629aac1f025357a629e`.
All three owned games exit zero, on the actual Radeon renderer.

The independent archive decoder validates every directory entry and rolling
checksum, follows both serialized car lists to locate history and crews, and
checks the cash, car configuration, single history step and canonical samples.
The old first/late saves differ in exactly 12 CMS byte values, confined to the
history and one crew's respect. Across independent old/fixed race runs, tiny
tyre-wear differences are expected from their different race durations; no
whole-file equality claim is made between those runs.

Five new summary cases cover normal continuation, skip-all, disabled saving,
meeting-only, and meeting followed by respect, including the 50-point history
ring wrap, host-loss clamp and respect floor. They snapshot `Career::write` at
the actual autosave request boundary and decode through the real archive and
Career readers before the later leave can conceal stale state.

- Same regression suite with original two summary translation units:
  **42 passing / 73 failing assertions**. Those units are compiled against the
  new compatible host signature with its default false argument and linked
  before the fixed library; this negative test binary is not an untouched game.
- Fixed suite: **115 passing / zero failing assertions**.
- Linux CTest: **10/10**. Android host CTest: **4/4**; these host checks are not
  physical-device or Android summary-runtime tests.
- Full integration with the existing independently resolved model inventory and
  matching controlled stock-save fixture: **371 passing / one failing check**.
  That strict check still reports **428 BOH / 7,929 D8M / 10,034 PMD**, including
  the original **119 D8M parse failures**. It remains a failure.

Earlier harness runs with no profile, an added-car save against base-only
definitions, or without the DAT inventory are retained with their extra
failures. They are not substituted for the final full gate. An initial copied
binary lacked executable mode, so Popen failed before any PID existed; that
attempt is explicitly excluded. Initial test-only type/path build errors are
also retained. No production code was reverted to obtain negative evidence.

## Android candidate and packaging state

`out/releases/OpenJuice-career-save-arm64.apk`: versionCode **24**, versionName
`1.02.5-android.23-career-save`, **10,950,770 bytes**, SHA-256
`59ace3a07672f1bc50430eec97e1bda391c4bacbc1a37c67a012ed0c25d409c5`.
The release build recompiles all three changed summary/continuation units. Its
actual ARM64 `libmain.so` exports the new `afterRaceResults(bool)` symbol, unlike
v23. Signature verification retains certificate
`a7ebaad7de021b73f8498f9b60c81c601e321560e362de223cf2d4fb33741e35`.
All seven native libraries pass API-26 dependency and ELF 16-KiB checks; ZIP
alignment passes. No RP5 interaction or new Android emulator execution occurred.
Physical RP5 remains APK19 / bundle r5; local assets remain bundle r6.

The independently packaged r10 Nix runtime and exact-source archives now also
contain this correction; see [actual Nix verification](nix-r10-save-verification-2026-10-31.md).
The habitual launcher continues to use the fixed development build. No release
publication is implied. Broader art/gameplay, campaign, Android and119-D8M work
remain open. The broad goal remains active.

Evidence folder: `out/validation/linux-current-20261029/post-race-save-order/`.
It contains original sources/ELF, scoped diff, positive/negative tests and build
recipes, controlled archives, gallery, APK audits and `verification-receipt.json`.
The actual UI process logs/captures remain under `full-race-ui/career-results-*`.
Six personal Linux save files and installed Wave 2 files retain their hashes.

```sh
python3 out/validation/linux-current-20261029/post-race-save-order/verify.py
```
