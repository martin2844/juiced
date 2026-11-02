# First-wave workshop purchases and saves — checkpoint, 2026-10-31

The appended r9 body choices now have normal-UI purchase/autosave/reload evidence,
beyond the earlier native model and archive-codec checks. Integra, BMW and Levin
each bought and reloaded indices 2 and 3 for front bumper, rear bumper and skirts:
**18 distinct choices, six saved/reloaded variant sets**. This closes that bounded
workshop requirement in `ten-car-acceptance-2026-10-31.md`.

The production engine and r9 package are unchanged. No Android interaction,
personal profile edit, new release, commit or Git push occurred in this step.

## Actual flow and money

Nine serial native-game processes start at the title screen, load Player through
the driver menu, continue Career, choose Workshop and the car, enter customization
and buy through the ordinary confirmation dialog. A virtual SDL pad supplies
observed press/release pairs. There is no autopilot, `--test`, deep-screen bypass
or runtime game-field injection. Profiles are owned synthetic one-car fixtures,
seeded with $1,000,000 and unlocked parts before launch; this does not certify
natural unlocking or earned progression.

| Car / saved set | Transactions leading to that save | Final money |
|---|---|---:|
| Integra kit3 | Both kit2 and kit3 bought per category: 6 purchases | $980,500 |
| Integra kit2 | After fresh kit3 load, kit2 bought again per category | $971,500 |
| BMW kit2 | Three purchases at $3,000 | $991,000 |
| BMW kit3 | After fresh kit2 load, three purchases at $3,500 | $980,500 |
| Levin kit2 | Three purchases at $3,000 | $991,000 |
| Levin kit3 | After fresh kit2 load, three purchases at $3,500 | $980,500 |

There are **21 confirmations** because Integra deliberately rebought kit2 after
its initial mixed sequence. Its different final balance is accounted for, not a
refund or a lost save. Every recorded quote matches the expected sequence. Each
changed set exits the garage through the real autosave. A separate process loads
it and reopens all three categories; owned parts show $0 and the saved geometry.

## Independent inspection and visual evidence

Evidence root:
`out/validation/linux-current-20261029/full-race-ui/ten-car-workshop`.
Open `gallery/index.html` for screenshots and the three reviewed contact sheets.

`verify_archive.py` reads the retail archive directory and rolling checksum
independently of the C++ save codec. For each retained actual UI save it checks:

- One correct model, exact body indices `[N,N,0,0,N,0,0,0,127,0]`, N = 2 or 3.
- Exact money deduction and matching career/car spending counters.
- Only those money/spending/body fields differ in `CMSGlobal.sav`; every other
  career byte and every other archive entry remains identical to its seed.
- The final reload-only process for each car leaves its saved archive identical.

`verify_cases.py` additionally checks native title/load/workshop logs, the saved
inputs recorded for each fresh process, all price quotes, observed neutral input
releases, executable/probe hashes and terminal status. All **nine game processes
exit 0**, and their owned Xvfb servers are terminal. It validates **114 PNG/BMP
pairs pixel-for-pixel**. All six reloaded part views per car were inspected in
three contact sheets; matrices compare both new variants in the actual workshop.

These sessions use **llvmpipe under private Xvfb**, not the Radeon capture setup
from the prior asset review. They establish UI/save behavior and bounded visual
presentation, not Android performance, hardware FPS or physical control feel.

The initial Integra entry helper expected `carsel` before confirming the car in
`CMSCarCollection2`. Its wait timed out. The same live process was inspected and
continued through the missing selection; the original failure log is retained.
All purchases occur after the corrected navigation. One generic reload capture
named `workshop-stock.png` actually shows reloaded kit3; the gallery corrects its
caption. No failed navigation or misnamed frame is counted as a purchase proof.

## Preservation and remaining scope

The six personal Linux files remain equal to the preceding checkpoint. Ferrari,
Wave2 and installed Street Cars payloads, production executable and complete r9
ZIP retain their hashes. Private fixture saves are retained as evidence and never
installed over a personal profile. See `verification-receipt.json` for hashes,
transaction sequences, archive inspections and reload references.

Owned raw BMPs are preserved on the secondary disk with verified, fsynced copies;
their original paths remain usable as symlinks and PNGs stay local. The offload
receipt records exact bytes and destinations. No game is running during offload.

Factory specifications/handling, the full source-art and body-kit styling quality
target, natural progression, and physical RP5 deployment/visual/driving approval
remain outside this bounded proof. The overall ten-car objective remains active.

Source inspection of availability found no new catalogue ceiling: ordinary
`newCareer` initializes every roster model, race-result unlock routing accepts
IDs below `CareerProfile::kModelCapacity` (64), and the unchanged retail step
table raises shared front/rear/skirt levels through 0, 1 and 2. `ModMenu` permits
indices 1, 2 and 3 at those respective levels. This establishes code-path
compatibility, not a claim that these funded fixture sessions earned the levels
through racing. No progression code or unlock recipe was changed.
