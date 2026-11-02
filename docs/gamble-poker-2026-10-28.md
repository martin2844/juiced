# Career / Gamble — playable preview, 2026-10-28

Requested direction: Texas Hold'em, the player and up to four Juiced rivals plus a dealer,
a 3D table, original talking-head reactions, and cash and car titles in the same game.
This is a new native engine feature, not recovered retail gameplay or an asset-only mod.

## Playing

Career now has a **Gamble** entry. Choose two to five players (including yourself), cycle
the rival lineup, choose a cash buy-in, and optionally select a garage car as a title stake.
Buy-ins are $1,000, $5,000, $10,000 and $50,000. Blinds are 1/100 and 1/50 of the buy-in.
The first career introduction must be completed; a race already in progress blocks entry.

Cash and titles coexist in **one monetary pot** (updated following the first playtest).
Selecting a title during setup only makes it available; it is not automatically wagered.
During your turn, choose **Bet / raise...**, cycle the cash amount, optionally add the
pink slip, and submit. A separate default-No confirmation names the actual car, its
sale value, the added cash, and the amount entering the pot. The retail `saleWorth`
calculation includes parts and damage; production always supplies the loaded catalogue.
Core tests can omit it to use a fixture's base definition value.

- A title can be wagered alone or with cash, once per seat per hand.
- Minimum raises, short calls, all-ins and side pots apply to the **combined cash value**.
- When a title covers a call but cannot make a legal raise, its excess value becomes
  change in the table stack. Subsequent bets can spend that change.
- The title is an asset funding the wager, not an additional winner-takes-all side prize.
- At settlement, each car's contribution is attributed to the exact pot layers it funded.
  Uncalled portions remain with their contributor; a short-stack winner cannot claim a
  more expensive car for free.
- Fully refunded titles return first. Remaining cars are allocated in descending value
  to an entitled recipient able to cover their value with their final table balance.
  The largest entitled share has preference; equal shares prefer the original owner,
  then seats clockwise after the button. The car's value is deducted from that balance.
- If no eligible recipient can cover the whole car, it is liquidated and the monetary
  shares are paid. No outside cash or debt is imposed. This makes split pots and ties
  preserve their exact monetary entitlements without duplicating car value as cash.
- An unpledged car stays in its owner's garage even if the owner folds.
- The garage must have space for every possible prize before a title-enabled hand starts.
- Cash-only all-in commits only the cash stack; it does not automatically wager a title.
- A hand must finish before returning to Career. The dealer button rotates between hands.
- Results remain in a large banner over the table, showing win/loss/break-even, net
  cash-plus-car value, cash change, cars received and whether the player's car was lost.

Rivals' cars come from the retail career's shared opponent-car pool, selected near the
player's staked car value and respecting crew make/region preferences, with crew paint.
The last race's `leaderCar()` is deliberately not used: outside a race that lookup can
return car zero for multiple crews. As in the retail pink-slip system, NPC cars are pool
instances, not persistent individual inventories. NPC cash stacks reset each hand; the
player's cash and garage are persistent. Poker does not increment the campaign's
pink-slip challenge counters or change respect.

## Visual direction and assets

Native Career blue/pink tribal frame textures, Concielian heading and Goodtime labels;
cyan focus and turn indication; a purple felt table with a dark rail and warm metal edge.
The layout reserves the center for the dealer and board, the bottom for the player's hole
cards and status, and the right for controller-driven actions. Palette: dark room #110e1b,
felt #342744, rail #16131d, edge #b57a41, focus #00b4ff, ivory cards #f5efde.

The table, supports, deck, cards, chips and button are native geometry with perspective
projection and GPU depth testing through `IRenderDevice`; cards animate from the deck.
This is a fixed-camera scene. The dealer is represented by an unused original crew
portrait, not a newly modeled/animated human body. Rival portraits and movie clips are
original retail assets. No game assets are embedded in the APK or added to this repository.

The retail movie system has one current UI player. One rival speaks at a time; other
seats show their original portraits. Greetings, accepting/declining bets, acknowledgements
and cash results use the existing clips and speech-volume setting. No synthetic voice or
invented dialogue. Clips are reused as recorded, so some wording can retain racing context.

## Implementation and save boundary

- `src/Core/Gamble/Poker.*`: deck, legal action order, seven-card evaluation, blinds,
  betting rounds, minimum raises, cumulative short all-ins reopening action, side pots,
  tied pots and odd chips. No UI or career mutations.
- `src/Core/Gamble/CareerPoker.*`: validates funds/cars/capacity, snapshots the stake,
  verifies the career has not changed, applies a completed result once, and rejects
  repeated or premature settlement.
- `src/Game/Frontend/Career/Gamble.*`: table setup, confirmation, actions, rivals, original
  movie reactions, result and normal career autosave request.
- `src/Game/Frontend/Career/PokerTable.cpp`: built-in screen and procedural 3D scene.
- Career menu entry and Frontend state dispatch wire the feature into the existing flow.

The AI samples unknown cards using only its own hole cards and the public board; its
decision function is not passed opponents' cards or the shuffled deal. Cash in play is
bounded by the explicit buy-in plus the value of any explicitly wagered title.

No additional career save fields or new save-directory scheme. The original career bytes
remain unchanged during a hand. The completed net cash result and car ownership changes
are applied together, then the existing autosave window is requested. If the process dies
before settlement/save, the last persisted career remains the recovery point; unfinished
hands are not resumed. Existing saves are neither migrated nor replaced by this feature.

## Verification

All gameplay checks use synthetic careers; no user save is opened or wagered.

Unit coverage includes ranking/kickers, ace-low straights, duplicate cards, heads-up and
multiway order, rejected raises, short and cumulative all-ins, side pots, odd-chip ties,
2,000 randomized chip-conservation hands, once-only career settlement, insufficient funds,
missing titles, full garage, tied title return, monetary title wagers, title change,
indivisible split-title liquidation, 400 randomized asset-conservation hands, concurrent cash changes, and legacy
career serialization round trips.

`tools/PokerCheck.cpp` loads the real retail data, renders the native menu and table,
decodes an original talking-head clip, checks default-No stake confirmation, plays a
mixed cash/title hand to settlement, observes the autosave request, verifies the saved
career reads back, and checks Career menu re-entry does not duplicate Gamble.
It does not exercise the actual save-device write or physical-device audio/controller input.

Reproduce on NixOS:

```sh
nix-shell shell.nix --run 'cmake --build out/linux --target juiced juiced_tests juiced_poker_check -j4'
out/linux/juiced_tests --unit /tmp/openjuice-poker-unit-saves
out/linux/juiced_poker_check /path/to/Juiced /tmp/openjuice-poker-captures
nix-shell shell.nix --run 'cmake --build out/android-host --target android_poker_check -j4'
out/android-host/android_poker_check /path/to/Juiced /tmp/openjuice-poker-android-captures
```

Local evidence: `out/poker-validation/` and `out/poker-validation-android/` contain setup,
confirmation, preflop, flop, result and native Career-entry captures. Build the APK with
`tools/build-android.sh release` inside `android-shell.nix`'s FHS environment.

Status: Linux and Android-renderer host checks pass; Android ARM64 APK compiles. Physical
Retroid playtesting and macOS compilation are not verified in this checkpoint. This is
a playable preview for review, not a published release or a finished art pass.

Final local validation: **243 unit checks passed, zero failures**; both native and
Android-renderer host integration checks passed after the final reaction-clip fixes.
Logs are retained alongside the captures (`unit-tests.log`, `integration.log`,
and `apk-build.log`). The signed ARM64 preview is
`out/releases/OpenJuice-Gamble-preview1-arm64.apk`, with an adjacent SHA-256 file.
Version: `1.02.5-android.11-gamble-preview1`, code 12. It has not been published or
installed on a physical device. The connected device at packaging time was an iPlay
70 mini Pro, not the Retroid Pocket 5, so no installation was attempted.

## Valued-title follow-up

The first preview's separate title side wager is superseded by the monetary rules above.
The launcher uses `out/linux/juiced`, so rebuilding that target updates the normal Linux
entry point. New visual evidence is in `out/poker-value-validation/` and
`out/poker-value-validation-android/`; these checks explicitly submit a mixed wager and
verify the large result banner. The original preview1 APK mentioned above is historical;
use the latest preview2 artifact for the valued-title behavior.

Follow-up validation: **275 unit checks passed, zero failures**, including physical
title transfer, twice-pledged rejection, cash change from a title call, exact split-pot
liquidation and 400 randomized cash-plus-car conservation hands. Both renderer-host
integration runs passed the mixed wager, cancellation, confirmation, result banner and
autosave-request checks. Captures were reviewed at 1280x720. No user saves were accessed.

Signed ARM64 APK: `out/releases/OpenJuice-Gamble-preview2-arm64.apk`, with adjacent
SHA-256; version `1.02.5-android.12-gamble-preview2`, code 13. Built successfully, not
installed or published. Linux's ordinary launcher now runs the rebuilt binary with
these changes; an already-running game must be closed and reopened.

## Card-face readability review

Replaced the three-column rank glyphs with clearer five-column, seven-row glyphs.
Card faces now use the same rank label as the gameplay text (`10`, never `T`) and
show it in both opposing corners. Spades have rounded shoulders and a stem;
clubs have three clearly separated round leaves. Diamonds/hearts remain red and
clubs/spades black. Card IDs, suits, shuffle and hand evaluation are unchanged.

`PokerCheck` now captures all 52 actual gameplay meshes through
`makePokerCardGallery`, at their correct aspect ratio. Reviewed the complete sheet
and in-table cards; both Linux and Android-renderer host integration runs passed.
Evidence: `out/poker-card-validation/` and `out/poker-card-validation-android/`,
including `all-52-cards.png`, `mixed-wager.png` and `integration.log`.
Linux is rebuilt and available through the ordinary launcher. The previously
packaged preview2 APK predates this visual correction; no APK was repackaged in this pass.

## Selected-title car preview

Added a compact native `CarView` in the lower-left panel, visible while selecting or
wagering a title, with the selected garage car's model and name. It uses a stable copy
of the actual car (paint/parts included), the dealership lighting/environment, a fixed
three-quarter camera, and the miniature viewport's own aspect ratio. Loads are queued
only when the selected garage index changes; a loading/unavailable label prevents an
empty frame from looking like a completed preview. The preview hides for cash-only
selection and on hand completion, and its resources are released when leaving Gamble.

Linux and Android-renderer host integration passed with real Celica and Civic models:
initial loading, switching cars/names, removing the title selection, restoring it,
mixed-wager confirmation, settlement, and hiding the preview after settlement. Reviewed
`selected-title-car.png`, `second-title-car.png` and `mixed-wager.png` under
`out/poker-car-preview-validation/` and `out/poker-car-preview-validation-android/`.
These use synthetic careers only. Linux's ordinary launcher picks up the rebuilt game;
the APK has not been repackaged or installed for this UI follow-up.

## Big review and reported-bug fixes — 2026-10-28

### Verdict: Comment — one Medium finding remains

Scope: the uncommitted Gamble implementation, core poker and career transactions,
UI/rendering/preview, Frontend and UI-file integration, tests, seed helper, and build
wiring. Two independent read-only review passes covered core rules/settlement and
UI/lifecycle; candidates were checked against callers and reproduced before reporting.
The earlier Android SaveEditorActivity change is outside this poker review. No files
were dismissed as formatting-only. This is not a review of the entire historic port.

### Medium findings corrected in the requested fixes

1. **Menu entries accumulated across real returns.** `CareerMenu::enter` removed only
   the visible Gamble entry, then `addCommandItem` appended to both visible and retained
   lists. `Frontend::baseEnter` restored the retained list on the next visit. The old
   two-enter test omitted that restore. The corrected code removes all matching visible
   entries and inserts the new item with a nonnegative index, leaving the retained stock
   menu unchanged. The new test restores both Menu and BackMenu before each of 20 visits;
   it failed on the third visit before the fix and passes after it.

2. **Small previews loaded invisible models and serialized GPU work over many frames.**
   `CarView::load` constructed a mirrored car, neon and shadow even when disabled by its
   immutable Setup. They now load only when used. `Gamble::framePreview` also services
   the native GPU upload queue briefly while a car is loading, with a 2 ms scheduling
   budget per call; a single driver upload cannot be preempted by that budget. Previously
   each serial upload could wait for the next frame. Reflections/shadows remain available
   to other car views; an integration test checks their listed models and verifies that
   compact views issue fewer vertex-buffer uploads.

Controlled desktop measurement, same two stock cars and a 16 ms service cadence:

| Selection | Linux before | Linux after | Android renderer on desktop after |
| --- | ---: | ---: | ---: |
| Celica, initial | 3849 ms | 185 ms | 108 ms |
| Civic, next selection | 4293 ms | 182 ms | 88 ms |
| Celica, return | 3339 ms | 70 ms | 42 ms |

These are local elapsed-time samples, not Retroid measurements or guarantees for every
modded model. Evidence is retained under `out/poker-review-before`,
`out/poker-review-after`, and `out/poker-review-android`.

### Historical finding, corrected 2026-10-30: short opening all-in reopens a checker

`src/Core/Gamble/Poker.cpp:169` records `lastFacedBet = -1` and leaves `mayRaise = true`
after a check. A subsequent opening all-in smaller than the big blind therefore lets
that checker raise, although the action has not reached a full bet.

Reproduction: stacks `{1000,1000,150}`, dealer 0, big blind 100; all call preflop;
on the flop seat 1 checks, seat 2 goes all-in for 50, and seat 0 calls. Seat 1's raise
to 150 is accepted. Verified using the current core in the standalone reproduction
`out/poker-review-after/short-opening-allin.cpp`; its output is saved alongside it.
The [TDA's explicit example](https://www.pokertda.com/forum/index.php?topic=736.0)
and [rule 49A](https://www.pokertda.com/view-poker-tda-rules/) require a full bet/raise
to reopen action for that prior checker.

Fix proposed by the review: record zero as the bet faced after checking and close raising;
reserve -1 for seats that have not acted. Reopen when a full wager is faced. Add regression
coverage for the prior checker, a player who has not yet acted, and subsequent full-bet
reopening. This rule change was not applied as part of the menu/loading fixes.

### Checked without further confirmed findings

- Ranking, side pots, chip conservation, car-value accounting and monetary compensation,
  once-only settlement, unchanged save layout, original garage preservation and capacity.
- Modal confirmation cancellation, stable preview-car lifetime, asynchronous cleanup,
  autosave Push preserving Gamble state, and lazy original-movie loading.
- Seed helper staging/roundtrip behavior and target wiring. Physical Android and macOS
  runtime behavior remain outside this local validation.

Validation after the fixes: **275 unit checks and 17 CarView integration checks pass**;
both native renderer integration runs pass, including 20 menu restore/re-entry cycles,
title preview changes, cancelled/confirmed wagers and settlement. No user save was used
by these tests. Linux's normal launcher uses the rebuilt executable. No APK publication
or installation was performed.


## Correctness and actual-save follow-up — 2026-10-30

The short-opening-all-in finding above is now corrected with before/after
regressions. A second correction makes per-title contribution attribution
additive across intervals and consistent with each pot's actual odd-chip payout.
Core Poker passes 55 checks and CareerPoker 48; 2000 chip/attribution and 400
mixed-asset randomized hands are included. Nine native and four Android-host
CTest suites pass. Actual normal-frontend cash and mixed-title hands now verify
two save-device writes and separate-process reload, beyond the earlier fake-host
autosave-request coverage. No user profile was wagered.

The latest local built/audited Android candidate is code20,
`OpenJuice-poker-rules-arm64.apk`; earlier previews above are historical.
It has not been installed on RP5, which the user is using. The complete current
integration still reports the unrelated original 119-D8M inventory failure.
See poker-closure-verification-2026-10-30.md for source/binary identities,
reproductions, exact outcomes, delivery checks and remaining limits.
