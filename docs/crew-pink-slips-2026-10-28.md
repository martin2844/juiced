# Crew Pink Slip Races — 2026-10-28

## Behavior

New optional Lua package `mods/crew-pink-slips` enables `crew_pink_slips`.
A career pink-slip duel with team size one and recruited crew members now opens
the existing crew driver selector. Choose yourself or a teammate, then assign one
eligible garage car. A teammate races under AI control; the player can watch and
use the existing intensity commands. No extra cars are added to the event.

The existing hosted pink-slip requirement (600 respect) is unchanged. Ordinary
pink-slip invitations also use this selection. No save schema or roster changes.
The engine's usual title transfer remains in use, including the pre-race autosave
that temporarily removes the staked car. No real saves were changed in testing.

## Integration corrections

- AtMeeting allows the crew selection screen when the package is enabled.
- RaceSetup's Back button returns to that selector under the same condition.
- GridWalk treats a crew entered by the player as a title wager even when the
  human is not driving. It returns immediately after requesting title confirmation;
  previously a subsequent loading request could supersede that confirmation.
- CharacterScreen initializes the race after title acceptance, instead of relying
  on that earlier GridWalk fall-through.
- `leaderCar` resolves the actual lone crew racer for delegated pink-slip previews,
  rather than falling back to an unrelated car in garage slot zero. This resolution
  follows event/session data so disabling the package does not corrupt settlement.

## Validation and artifacts

Android-host integration uses retail assets and a synthetic career: driver
selection, AI racer identity, nonzero garage index, confirmation transition,
win/loss transfers, disable behavior, and existing hosted respect/race-type tests
pass. Linux unit suite: 275 passed, 0 failed. Linux executable rebuilt successfully.
Signed ARM64 APK built successfully (versionCode 15).

- `out/crew-pink-validation/integration.log`
- `out/crew-pink-validation/unit-tests.log`
- `out/crew-pink-validation/SHA256SUMS`
- `out/releases/OpenJuice-crew-pinks-arm64.apk`
- `out/releases/OpenJuice-crew-pink-slips-mod.zip`

Android: update the APK and import the mod ZIP in Mods, enable and restart. The
APK alone does not seed the new Lua package. Linux's normal launcher uses the
rebuilt executable and repository mod defaults. These builds also include earlier
uncommitted changes in this workspace. No GitHub publication or device installation
performed. A full physical-device race and visual confirmation remain unverified.
