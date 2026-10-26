# Ferrari and save-editor corrections — 2026-10-26

Android 1.02.5-android.6 (versionCode 7), Ferrari package revision 2.

## Findings and changes

The Android .5 save editor called makeCar without rateCar. The dealer calls both.
New editor cars now run the same dynamics/catalogue rating calculation. Career
load recalculates a zero rating after binding the model definition, recovering
already-added cars while preserving parts, damage, money and race history.
The editor also excludes the engine's NONE placeholder from addable vehicles.

The Ferrari's stock tyre diameter is about 71.60 cm; the first retail replacement
wheel is about 64.96 cm. The renderer previously swapped meshes without fitting
them. Imported cars can now opt into MATCH_STOCK_WHEELS in their definitions.
The Ferrari opts in. Shared wheel and blur meshes are fitted to stock width and
diameter before GPU upload, including inverse-transformed normals and adjusted
bounds. Retail cars keep their original behaviour. This preserves the outside
tyre envelope, not an identical rim design or rim-to-tyre ratio across models.

The carbon bonnet used coarse scratch UVs and an unlit Standard Opaque material.
Its outline also followed a centroid-based triangle partition, creating a jagged
edge. The revised carbon insert clips triangles against a clean tapered outline,
interpolates their attributes, retains the original surface and painted margins,
uses fine twill UVs and a dedicated specular material. Stock bonnet assets are
unchanged. This is an authored mod part, not a scan of a specific aftermarket hood.

The Ferrari originally had only stripes, side graphics and an imported-image slot.
Its catalogue now appends all 31 retail styles on fitted Ferrari decal surfaces,
referencing original materials/textures in the owner's archives. Existing decal
indices 0–2 remain stable. Menu prototype locks, tint availability and prices use
style identity, rather than assuming the first entry is always a prototype.
Auto-Mod avoids selecting the player's custom image when its slot is in the middle.

## Verification and evidence

- `android_save_editor_check`: every real vehicle in the active catalogue gets
  positive power when added; existing archive, invalid-edit and restore checks remain.
- `android_mod_garage_check`: 202 regular and blur wheels preserve width and tyre
  diameter within 0.02 cm, in both garage and race models. All 34 decal meshes,
  materials and textures load; carbon material loads; stock Ferrari rating exceeds 500.
- Android renderer host screenshots compare stock and carbon/wheel/vinyl variants.
- Retroid Pocket 5 screenshot shows 567 bhp, carbon, a replacement wheel and the
  retail Tribal VS1 graphic. The debug-only garage-check entrypoint renders a
  deterministic selector snapshot. Its temporary collection override was restored.
- Player and Ronnie juicey.dat files compared byte-for-byte against pre-test
  backups and were unchanged. Existing zero-rating repair is performed in memory
  when a career is loaded and is persisted by the normal game save flow.

Local, ignored evidence: out/validation/android/ferrari-garage-check.log,
ferrari-editor-check.log, ferrari-stock.png, ferrari-carbon-vinyl.png,
ferrari-retroid.png, ferrari-release-build.log. Private save backups stay local.

The APK and both the internal Ferrari package and Documents/OpenJuice/mods source
folder must be updated together. Asset fixes are not embedded in the APK.
All catalogue entries were checked structurally; every vinyl was not individually
judged in a driving scene. No performance/framerate claim is made by this change.
