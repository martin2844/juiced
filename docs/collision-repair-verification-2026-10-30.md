# Physical collision, saved damage and production repair — 2026-10-30

Follow-up 2026-10-31: the post-race respect/history discrepancy described below
is corrected and independently reproduced before/after. See
[post-race save verification](post-race-save-verification-2026-10-31.md).
The immutable collision/repair checkpoint retains its original evidence and scope.

The modified Peugeot 406 completes a real Linux collision → Career autosave →
fresh-process damaged workshop load → repair → autosave → fresh-process repaired
workshop load. This extends the [actual workshop purchase proof](workshop-save-reload-verification-2026-10-30.md).
No production sources, installed assets, APK, launcher or personal saves changed.
The RP5 was not accessed, following the user's instruction to continue on Linux.

## What actually ran

The owned fixture retains the previously UI-purchased 406, body indices
`[3,3,1,0,3,0,0,0,8,0]`, wheel 47, 191.684 BHP and 985,400 cash. A synthetic
one-lap Engstrom circuit event has two competitors, no entry fee, no prize and
no titles. Only the initial calendar is seeded; no race damage or repair result
is written by the fixture. This is not naturally earned career progression.

The first game is a separate diagnostic binary, linked with one out-only copy
of `RaceCar.cpp`. Read-only snapshots bracket the existing physical contact's
native `DamageGrid::hit` call. The original simulation call, control flow and RNG
calls remain in place. Logging overhead means this is not a performance or
trajectory-equivalence measurement. The production executable stays unchanged:
`38ad6cfb6f8948defacfd6713a3e4b1f367ac08523543671353db68e7dca4a1e`.

The player is slot 1, model 58; slot 0 is the AI Corrado. Virtual SDL acceleration
produces actual circuit-boundary contacts. Three observed hits move both body
and decal vertices; the first moves 5,726 body and 2,116 decal vertex references,
up to 3.3774 cm. All observed positions are finite. Some contacts move no
registered damage vertices; those are retained, not counted as positive dents.

The diagnostic field `other_attributes_preserved` hashes normals and UVs
together. Native `CarDamage.cpp::moveVertex` intentionally perturbs/normalizes
normals. Therefore its zero value on deformed vertices is expected and is not
evidence of UV corruption or proof of UV preservation. A separate UV-only
measurement would be needed for either claim.

The player becomes stuck against the boundary and abandons normally via pause
and the confirmation. The game records second/last place and zero winnings,
shows the respect updates, autosaves and opens the workshop. This does not prove
a completed player lap or victory. `before-steering.png` is explicitly not an
undamaged baseline: the earlier collisions already happened.

## Persistence and repair

The real autosave has 26 nonzero saved damage-grid bytes, total 310, versus zero
in the starting fixture. No damage is injected into the archive. The diagnostic
workshop visibly shows the bent vinyl/body and quotes a 500 repair.

An ordinary production game, started separately through title/driver/load,
reads that exact damaged archive. Its workshop again shows the damaged car and
500 repair quote. Rejecting the quote and leaving leaves the entire archive
byte-identical. A later confirmation charges 500 once: 985,400 → 984,900. The
repair option disappears, the body/vinyl return to their undamaged shape, and
the normal `autosave over carsel` / `autosave closed` path writes the result.

An independent Python decoder validates the complete archive directory and
rolling checksum. Between the production damaged and repaired saves, only
`CMSGlobal.sav` changes, in exactly 38 byte values confined to:

- Cash, profile money spent and this car's money spent (+500).
- Native tyre-wear reset and full-nitro refill (15).
- The 256-byte saved damage grid, now entirely zero.

All other career bytes and archive entries are identical. Model, body parts,
vinyl/color, wheel, tuning, rating, roster and progress remain unchanged. Nine
negative controls reject changed cash, hood, wheel, power, retained damage,
unrelated career history, unrelated archive data, checksum and trailing bytes.

A third game, also the ordinary production executable, loads the repaired
archive and renders the repaired 406 with 984,900 cash. Its complete save archive
remains byte-identical. All three owned game processes exit zero. Six personal
Linux save files and installed Wave 2 files retain their previous hashes.

## Limits and follow-up

The uninterrupted diagnostic session's later repair save also differs from its
earlier post-race autosave in the serialized history and one crew's respect
(248 → 424). `RespectChanges::leave` restores `respectNow`, and
`Frontend::afterRaceResults` records history after the earlier autosave. This
needs a separate post-race save-order audit; it is not silently included in the
repair's allowed differences. The strict repair proof uses the fresh production
load and preserves every unrelated byte. This checkpoint does not approve that
post-race metadata discrepancy.

Only one car/configuration has this full physical collision/save/repair path.
All-model damage coverage, close material quality, complete campaign progression,
physical Android validation and the original 119-D8M gate remain open. The broad
verification goal remains active. No new runtime release or APK is implied.

Evidence: `out/validation/linux-current-20261029/full-race-ui/damage-fixture/`:
fixture and observer sources/build receipts, controlled synthetic archives,
`verify.py`, `verification-receipt.json`, and `gallery.html`. The three process
folders retain input actions, Radeon readbacks, runtime logs and terminal proof.

```sh
python3 out/validation/linux-current-20261029/full-race-ui/damage-fixture/verify.py
```
