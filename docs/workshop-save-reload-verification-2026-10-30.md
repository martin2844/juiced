# Native workshop purchases, autosave and reload — 2026-10-30

The current Linux r9 development executable completes a normal title/driver
load/Career/workshop flow on an isolated synthetic profile. No production code,
car assets, APK or user saves were edited. Physical RP5 was not accessed.

The fixture contains one factory Peugeot 406 Coupé, native-rated 191.684 BHP,
1,000,000 cash and seeded styling unlocks. These are controlled test conditions,
not naturally earned progression. Only the fixture preparation tool writes the
initial profile; subsequent purchases, confirmations, autosave and reload use
the normal frontend and save device, driven by a process-local virtual SDL pad.
Actual Radeon rendering is captured at 1280×960. No FPS benchmark is implied.

## Actual workflow

| Confirmed purchase | Cash charged |
| --- | ---: |
| Front bumper, kit 3 | 3,500 |
| Carbon hood, choice 1 | 2,500 |
| Retail wheel catalogue ID 47 | 600 |
| Side skirts, kit 3 | 3,500 |
| Rear bumper, kit 3 | 3,500 |
| Retail vinyl 8, native colour chart | 1,000 |
| **Total / final cash** | **14,600 / 985,400** |

The 3,500 front-bumper quote was cancelled once and the preview abandoned;
the later purchase charged once. A 532 wheel quote was also cancelled before
buying wheel 47 for 600. Actual quote order is retained in the log and verifier.
The controller initially assumed a different carousel order; the shipped UI
resource establishes front/hood/wheels/skirt/rear/decal after unsupported items
are removed. The resulting wheel purchase is included in the proof, not hidden
or described as a skirt purchase. Two early screenshot filenames are corrected
in gallery captions and the receipt: `rear-purchased` shows the hood purchase;
`skirts-and-carbon-purchased` shows the wheel purchase.

Leaving the workshop invokes the real `autosave over carsel` / `autosave closed`
path. The game returns to the car collection and exits normally. A separate
process then selects the driver, loads the saved career, enters the workshop
and renders the modified 406 with its 191-BHP rating and 985,400 balance.
The first reload capture caught the black transition; it is explicitly excluded.
`reloaded-modified-ready.png` is the completed loaded scene.

## Independent save proof

An independent Python parser validates the archive directory, payload rolling
checksum, model name, fixed serialized fields and complete payload preservation.
Only `CMSGlobal.sav` changes. Exactly **16 byte values** change, confined to:

- Career cash and total money spent; `moneySaved` is transient, not serialized.
- This car's front/rear/skirt indices 3, carbon hood 1, vinyl 8 and wheel 47.
- Chosen vinyl colour `0xffbfbfbf` and the car's 14,600 spent counter.

All other bytes of the entire career payload, other archive entries, tuning,
power, roster and progress remain identical. The complete archive is identical
after the separate-process reload:
`e0cbedf53106287c602bc3edc5a4ad6db8d9bda13df8775a172238641b8b27a6`.
Eight negative controls reject altered money, hood, wheel, model, unrelated
career data, another archive entry, invalid checksum and trailing bytes.

The first harness attempt began at `menu`, bypassing title/driver loading and
starting a fresh introduction. It was stopped normally and the fixture file
was unchanged. It is retained as excluded evidence, not a successful workshop
run or a production defect. The successful run starts at the normal `start`
screen. All three owned game processes exited zero; none remains running.

Six personal Linux save files and the installed Wave 2 files retain their
recorded hashes. Current game ELF remains
`38ad6cfb6f8948defacfd6713a3e4b1f367ac08523543671353db68e7dca4a1e`.
No rebuild, new artifact version, publication or physical-device update was
needed for this evidence-only extension.

## Evidence and limits

`out/validation/linux-current-20261029/full-race-ui/workshop-fixture/gallery.html`
contains annotated full-size screenshots and the workflow contact sheet.
The same folder retains the fixture source/build recipe, controlled archives,
independent inspections, eight-negative verifier and verification receipt.
The two successful process directories retain command/probe hashes, actions,
runtime logs, captures and terminal receipts. The excluded attempt is separate.

```sh
python3 out/validation/linux-current-20261029/full-race-ui/workshop-fixture/verify.py
```

This proves one actual imported-car purchase/save/reload workflow. It does not
approve every car/category/colour/price combination, natural unlock progression,
in-race deformation, close material finish, or Android. Source-model artistic
limits and the strict original 119-D8M gate remain open. The broader goal remains
active; previous component/geometry/CarView evidence is not substituted for this
normal frontend workflow.
