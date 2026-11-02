# Poker correctness and real-save verification — 2026-10-30

This pass extends the 405-glazing checkpoint. It addresses the previously open
poker-rule finding, a second confirmed attribution defect, and the gap between
renderer-host autosave requests and actual save-device writes. The RP5 remains
in use by the user; no device control, installation or export was performed.

## Review scope and findings

Two read-only GPT Luna reviewers covered financial/asset invariants and frontend
state/lifecycle, following the big-review skill. The coordinator independently
read the implementation and tests and reproduced findings before changing code.
Reviewable scope: Core/Gamble, Gamble, PokerTable, career menu/dispatch callers,
unit tests and the feature document. Other historic port changes are outside
this review. No additional confirmed UI/lifecycle finding was admitted.

1. **A checked player could raise a subminimum opening all-in.**
   Poker.cpp used the same `lastFacedBet = -1` sentinel for a check at zero and a
   seat that had not acted. Stacks 1000/1000/150, big blind 100: after preflop
   calls, seat 1 checks, seat 2 opens all-in for 50 and seat 0 calls. Seat 1 was
   incorrectly allowed to raise to 150. The new regression initially reports
   **43 passed / 3 failed**. Recording zero after a check and closing its raising
   right until a full wager is faced gives **46 passed / 0 failed**. It also tests
   an unacted player, ordinary full opening bets, cumulative short opening bets
   and title funding not bypassing the raising restriction.

   The rule was independently checked against [Poker TDA 2026 Rule 49A](https://www.pokertda.com/view-poker-tda-rules/)
   and the [TDA administrator's explicit check/short-opening example](https://www.pokertda.com/forum/index.php?topic=736.0).
   No claim is made that this custom game implements every tournament procedure.

2. **Contribution attribution restarted odd-chip priority for each interval.**
   The actual cash pot was split correctly, but attributing two odd contributions
   could assign both rounding remainders to the same winner. Two tied 10001
   contributions paid 10001 each, while attribution summed to 10000/10002.
   Splitting a 10002 contribution into two 5001 intervals also changed its
   attribution. Career title allocation uses these shares, so inconsistent
   entitlement accounting is meaningful even though the cash pot did not mint
   money. The focused added regressions initially report **51 passed / 3 failed**.

   Attribution now follows one continuous chip stream per pot layer, ordered by
   contributor seat. Its interval offset determines the odd-chip recipient;
   subdividing a title/interval cannot restart rounding. Actual pot payouts and
   their normal clockwise odd-chip rule are unchanged. The focused suite passes
   **54/0**, then **55/0** with a seeded 2000-hand contribution/payout/additivity
   property added. The separate career suite passes **48/0**, including its 400
   mixed-asset conservation hands. The financial reviewer independently attempted
   to refute the offset handling for partial intervals, side pots, folded donors
   and refunded overbets, and found no counterexample.

The existing 3D table, card artwork, car-preview geometry, title valuation and
save format are unchanged by these fixes. A test invoked while its rebuild was
still running exercised the older test binary and is retained as
`old-binary-observation-during-build.log`; only the later terminal-build result
supports the corrected-code claim.

## Actual normal-frontend play and save-device writes

Development executable SHA-256:
`4e21db0ba62aab56383fed55488dc9149fd67364163d97c462f0ce4dc479789b`.
The former development binary is retained in the validation directory.

`career-poker-cash` boots through title, profile load and Career, then opens the
visible Gamble entry with a virtual SDL pad. It uses an owned synthetic career:
$40,000, Eclipse/GTR, Vito and eleven added-car progress records. No user profile
is opened or wagered. The game runs without `--test` or diagnostic autopilot;
opponents make their ordinary poker AI decisions. The SDL probe only injects
normal input and captures the framebuffer.

- A five-player cash hand finishes with a **$295 win**. The large result banner
  agrees with the actual autosaved archive: **$40,295**.
- A second, heads-up hand selects the actual Eclipse. Its compact preview and
  caption agree; the sale value is **$39,797**. The player composes a title plus
  **$980 cash** wager, total **$40,777**. The default-No confirmation is first
  cancelled, then explicitly accepted. The persisted career remains unchanged
  during dealing and after cancellation.
- The opponent calls with a short cash stack. The player loses the matched
  **$1,000**; the uncalled title value returns as the physical Eclipse. The result
  banner says the car returned; the actual archive contains **$39,295** and both
  original cars with byte-identical serialized records.
- Two actual `autosave over CMSGamble` / `autosave closed` workflows complete.
  The game returns normally to Career with one visible Gamble entry and exits
  through an orderly SDL quit. Both game and private display exit zero.
- A separate process loads the completed archive and displays **$39,295**.
  Loading leaves the archive byte-identical. Its capture includes menu-entry
  animation and is used for cash reload, not final menu-art approval.

Final owned archive SHA-256:
`f6fffef8468fd5ce41d851daf0ffe1c55506d2af76f541a797ed04ea7c3d378c`.
Independent archive/hash/profile inspection confirms unchanged garage, members,
crewmate statistics, calendar events, optional mod progress and non-career save
entries. Missing-phase controls reject an incomplete workflow. No opponent car
was physically transferred in these two actual hands; deterministic/randomized
core tests supply separate ownership/split-title evidence.

These private Xvfb/llvmpipe, dummy-audio and virtual-pad runs do not certify
physical touch/gamepad input, audible mix, Android rendering or FPS.

## Current build and delivery evidence

Full native build and CTest **9/9** pass. Android host build and CTest **4/4**
pass. The complete enabled current integration run is **260 passed / 1 failed**;
the sole failure remains the strict original model inventory's 119 D8Ms. It was
not suppressed or converted into a pass. Further renderer-host, package and
runtime evidence must retain their own binary identities.

Signed ARM64 APK: `out/releases/OpenJuice-poker-rules-arm64.apk`, code **20**,
version **1.02.5-android.19-poker-rules**, 10,950,770 bytes, SHA-256
`86138ab718ed834dc4bc08ccf07541f65842eaba715c32af0d4e92c6f63c1d77`.
It uses the same update certificate as v19, passes ZIP/ELF 16-KiB checks, and all
seven ARM64 libraries' dependencies resolve against packaged libraries/API-26
stubs. Only libmain.so changes from v19. Debug launch activity/native regression
contract entries are absent. This is a local built/audited candidate, not a
physical-device or publication claim. The separate data-only r6 car bundle is
unchanged. RP5 remains on APK v19 and car data r5.

The ordinary Linux launcher uses the new development executable. Nix r3 predates
these poker fixes; the fresh r4 Nix build/package has its own separate evidence
and must not be inferred from the development binary's tests.

Evidence roots: `out/validation/linux-current-20261029/poker-closure`,
`full-race-ui/career-poker-cash`, `full-race-ui/career-poker-reload`, and
`android-poker-rules`. The wider closure ledger remains open.


Both current renderer-host PokerCheck runs also pass after the fixes: native
Linux and Android renderer on this Linux desktop. Ten captures per backend
cover native previews, confirmations, table, all52 card meshes, result and
20 menu restore/re-entry cycles. All52-card sheets and mixed-wager/result frames
were manually inspected; PNG conversion preserves BMP pixels. These are host
checks with synthetic careers, not APK/physical-device runtime certification.
Their binary/log/image identities are in renderer-host-receipts.json.

## Later actual physical title-transfer evidence — 31 October 2026

`poker-title-win-verification-2026-10-31.md` extends the historical two-hand
scope above: current production ELF wins one ordinary heads-up title hand,
receives TK's exact211BHP Eclipse+$1000, autosaves, reloads in a separate process
and displays the prize in the workshop. Existing cars and all unrelated save
bytes remain intact; eight negative controls and source-car parity are separate
checks. Actual GUI tie/side-pot/other-AI breadth and Android remain open.
