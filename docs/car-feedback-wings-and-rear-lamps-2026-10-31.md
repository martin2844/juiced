# r17 — wings and reported visual defects

The user rejected the finish of the ten ports after playing r16: customization
was too plain, no wings were available, Volvo240 had an odd registration plate,
Jetta's rear lamps looked deformed, and405 lacked an interior. They confirmed
that the Jetta already looks wrong while parked in the workshop. General kit
artistry remains open; this delivery does not mark the whole ten-car goal done.

## Concrete changes

- All ten cars have three selectable native wing options, plus stock/NONE.
  Wings use actual retail Juiced geometry/materials:350Z VS1, Integra VS5 and
  350Z VS7 for sedans/coupes; Golf VS3/4/5 for hatchbacks/wagons. Their feet are
  fitted against measured exterior surfaces. Hatch/wagon placement was moved
  rearwards after reviewing native views. Existing source factory wings on
  Integra/E36/Levin/Golf/V70 are detached into their stock slot; the boot and
  hatch remain on the chassis. Other existing option lists keep their indices.
-405 keeps its existing glazing geometry and gets96% privacy glass. This is
  a fixed privacy material while the source interior is incomplete; the existing
  window tint slider does not change this separate material. No missing interior
  was claimed to have been modeled.
-240's downloaded-author registration is replaced with upright `JUICED`
  vector/text artwork, with a neutral plate backing. The first candidate exposed
  the source letter quad's inverted V convention; the final export maps bitmap
  top to world top and was reviewed in native front/rear views.
- Jetta's simplified intrusive trunk liner is identified by87 exact source
  faces and removed from12 native body/kit variants. Other interior components
  and exterior vertex data stay intact. This is separate from the lamp defect.
- Jetta's rear lamps restore16 original Blender source pieces, replacing2,009
  simplified/duplicate faces with8,348 original-source faces. Transparent lenses,
  housings and reflectors replace the flat white/whole-red surfaces. Brake
  illumination now uses324 actual brake-cell faces rather than lighting the
  whole outer lens. Flare anchors follow the real cells. A source-name based
  replay reproduces every runtime payload of the reviewed candidate exactly.

## Evidence

Evidence root: `out/validation/car-feedback-r17-20261031`.
Native wing review covers320 frames:stock and three options in eight directions
on each car. Five retained sheets came from wings03; final10 supplies the
three hatch/wagon sheets and the corrected240/Jetta sheets. A private native
brake off/on comparison adds12 before/after frames. These are actual CarView
renders on RX7800XT, not concept art or Android FPS evidence.

Full composed checks:1,322 Wave2 and483 first-wave assertions passed. The added
checks exercise custom wing loading/materials and ensure NONE removes only the
wing, preserving body panels. `STANDARD SPOILER NONE` follows the existing
retail convention, where the list's `standard` flag is false. Roster64, all
existing non-wing catalogue bytes, handling recipes and production ELF remain
unchanged. Native images still do not prove finished kit artistry or physical
RP5 behavior.

Rejected attempts remain recorded. The first rear-lamp candidate accidentally
wrote BOH bytes into the UI chassis because a temporary variable was reused.
Native preview warnings caught it; it was never installed. The repaired version
uses separate chassis bytes, has identical main/UI D8M payloads, and renders the
whole body. An early capture helper checked the GPU string in stdout even
though it was on stderr; this is corrected. Neither issue is counted as passing.

## Delivery and sources

Linux uses Street Cars8 / Wave2 15 / Ferrari4 through the usual launcher.
Both package exchanges retain r16 rollback copies. Repository runtime defaults
match installed files; six personal Linux files and seven unrelated packages
are unchanged. A post-exchange repository-copy error on newly added files was
recovered without repeating the exchanges or touching saves; see
`linux-r17-delivery.json` and `finish-linux-install.py`.

Runtime ZIP:`out/releases/OpenJuice-Car-Visual-Fixes-r17.zip`.
190,851,492 bytes;3,306 payloads; SHA256:
`efda933d0a100403665620c34119e1dd8625b1601c662700bd7945c9b2aea0c1`.
All ZIP CRCs and payload hashes were read back. With the user's device-availability
confirmation, r17 was imported through the RP5 launcher without replacing APK19.
The launcher validated64 cars and active mods; both street-car packs and Ferrari
remain enabled. Fresh save exports immediately before and after import contain
the same nine entries with identical SHA256 values, including Ronnie's profile.
Private backups are retained locally and in the device's Download folder:
`RP5-before-r17-valid-20261031.zip` and `RP5-after-r17-valid-20261031.zip`.
The device was left on Play with Start playing available. See
`out/validation/car-feedback-r17-20261031/rp5/delivery.json` and screenshots.
This verifies deployment and persisted saves, not visual acceptance in an RP5
race. No public release or Git push was performed.

The previous paused game held the import/export lock and was force-stopped
after the user freed the device. The first failed export produced an empty ZIP;
it was removed only after valid backups were verified. A mistaken original-game
launch was closed at its title screen without selecting a profile. Exported
persisted saves were unchanged by the subsequent mod import; an in-flight paused
race is not part of the save-backup verification.

Editable sources are in
`tools/street-cars-wave2/assets/user-feedback-20261031`:ten wing studies, the
restored Jetta lamp study, SVG/PNG plate and exact author scripts. Author commands
and original-source hashes are retained in their receipts. Rebuild steps take
the preceding r16 inputs for wing/category generation, then the targeted plate,
privacy, liner and lamp adapters; do not run the source-face replacement over an
already replaced r17 chassis.

## Remaining work

The user's criticism of plain bumpers/skirts is valid. R17 does not redesign
those kits. Next is a visible design pilot for layered fascia, grille/vent and
skirt detail, compared with actual retail Juiced kits, then adaptation across
the ten cars without changing vinyl receivers or hiding unsupported features.
Factory handling variant questions and final user device feedback remain open.
