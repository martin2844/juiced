# Peugeot405 fit and finish — 2026-11-01

The user judged the fit still unconvincing. R23 is a subsequent OEM-perimeter
rebuild, prepared pending installation; see `peugeot405-oem-fit-2026-11-01.md`.
R22's geometric and loading checks did not establish final artistic acceptance.

The user's workshop screenshot showed that r21 had corrected replacement
ownership but left oversized clearances and a rough finish. The previous
selector captures did not expose these defects adequately. This pass uses
the real `GarageView::enterSel`, `carsel` viewport and `don_garage1` scene,
including six front/rear close-ups and eight whole-car directions per state.
The same camera, lighting and paint are used before and after. The source
Peugeot model and the three previously approved kit directions are retained.

R22 / Wave2 revision20 is installed locally in Linux. It changes nine405
race/UI mesh pairs and four405-local material files. Other cars, catalogue
identities, parts, BOH transforms, wheels, hoods, skirts, wings, liveries and
physics are unchanged. The r20 approved102-to-400BHP preparation is retained.
The executable is unchanged, SHA256
`5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f`.

## Mesh corrections

- Front/rear mounting planes move to58.25/64.5cm, below the original lights.
  Exact clipping redistributes the factory surfaces between CHASS and stock
  slots while preserving their original area, UVs and normals. Custom kits
  continue to replace the factory shells rather than covering them.
- Dense mounting contours project onto the actual source surface, replacing
  the coarse convex-envelope fit. The collar is welded into the outer shell,
  with smooth shading and explicit outward winding. A failed intermediate
  candidate exposed Blender's automatic normal recalculation flipping open
  collar strips; that recalculation was removed and a native winding check
  added. The rejected study03 is not the delivery. Study04 is the final asset.
- The authoring wheel-arch clearance radius changes from39 to33.8cm around the
  unchanged wheel centres. This is a visual fit, not a physics change. Turned-in
  returns finish the shell edges. Redundant overlapping thin lower bands are
  removed instead of trying to hide their intersecting highlights.
- A conformal opaque intake lining follows the inside of the shell, offset
  along its normal, to block views through the ends of the ducts. The405's
  thin trim/rubber cavity materials become two-sided; shared materials and
  other cars are untouched.
- Flat constant-colour fog discs/flutes are replaced with round or rounded
  rectangular glass surfaces using the original headlamp texture's fluted
  reflector region. No new photographic texture or invented source detail is
  claimed. Lights-off appearance is checked in the actual workshop.

Native centimetres remain X across, Y up and +Z forward. Blender authoring
uses metres `(x,-z,y)/100`. Editable files under
`tools/peugeot405-fit-finish/assets` include the construction scene and a
complete four-scene assembly with packed original textures. Earlier model and
photograph attribution continues to apply; see the r19/r20/r21 documents.

## Evidence and delivery

- Independent exported-mesh read-back preserves factory surface area by
  material, finite attributes, valid indices, outward collar winding and
  byte-identical race/UI pairs. No factory outer bumper shell remains fixed
  beneath a custom option. Other assets/tuning match r21.
- Six fitted mounting contours are measured against the fixed body. Their
  worst exported mounting-vertex distance is0.013854cm, below0.2mm. This checks
  these geometric contacts; it is not a physical certification or a guarantee
  about every pixel, damage state or future modification.
- 70 final real-workshop views and64 selector views cover the kits, front/rear
  close-ups, light/dark paint and existing hood/wing/vinyl combinations. The
  native AMD renderer governs visual review; Blender materials approximate it.
- All1,322 native loading/refit, damage and compatibility checks pass on the
  final candidate. The habitual launcher loads a private copy of Tony through
  title/profile to menu with `--test --screen start`, without taking desktop
  focus or writing actual profiles.

Runtime: `out/releases/OpenJuice-Peugeot405-Fit-Finish-r22.zip`,133,419,000bytes,
SHA256 `380201587cbdf1790a1a450fe462d1ded761927ac3bf41b7176e94b0ae2379cd`.
Installed and repository-default packages match it. All six actual personal
files and other packages are unchanged, and an extra verified save backup is
recorded in the preparation receipt. R21 package rollbacks remain under
`out/validation/peugeot405-fit-finish-20261101`.

Gallery: `out/validation/peugeot405-fit-finish-20261101/gallery/index.html`.
This is a visual fit/finish pass, not final artistic approval, a race/FPS
benchmark or an Android update. The source cabin remains incomplete behind
the existing opaque glass; original texture quality, panel joints, workshop
shadows and the selector's floor-reflection artifacts remain visible.
