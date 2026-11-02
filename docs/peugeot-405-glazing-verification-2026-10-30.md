# Peugeot 405 glazing correction — 2026-10-30

The preceding saved-car review found entirely black/opaque windows on the 405.
The retained prepared scene assigns four connected window sheets to the black
`1994_405_Plastic` atlas, which exports as Specular Opaque. It has no native glass
assignment. The original Peugeot 206 asset independently uses Car Glass.

`tools/street-cars-wave2/repair_405_glazing.py` assigns only the four retained
window sheets (279 polygons) to `oj_glass`, selecting their exact source identity,
component sizes and cabin bounds. It preserves separate pillars/seals/mirrors.
No vertices are moved. The exported source retains all 9,990 triangles' positions,
normals and UVs to five decimal places, ignoring material regrouping. The original
prepared blend retains its hash. No synthetic cabin or geometry was substituted.

Evidence: `out/validation/linux-current-20261029/glazing405`.

Native Radeon RX 7800 XT comparison covers eight angles each of stock, carbon
bonnet, vinyl8 and maximum kit with carbon/vinyl8: 32 baseline and 32 candidate
images. Both contact sheets were manually inspected. The corrected windows show
reflection/translucency while the seals/pillars and body remain coherent. The
source still has limited interior detail; this is not an OEM interior/art-parity
claim. UI button-icon textures are not initialized by the gallery harness and
are excluded from UI approval.

The fresh three-package assembly passes **1,238 checks, zero failures**. All 32
packaged BMP captures are byte-identical to the reviewed candidate's captures.
BMP-to-PNG conversion preserves pixels. A separate actual packaged driving-model
sample on Engstrom_R1 reaches Go and applied AI movement with six 405s and exits
normally after 600 frames. Its chase-view image was manually reviewed. This is
a finite render sample, not a completed race or a performance benchmark.

Two diagnostic failures are retained: forced X11 was unavailable on this desktop
(the successful runs use Wayland/Radeon); the first candidate overlay updated
only the driving folder, so menu captures stayed byte-identical to baseline.
The passing candidate updates both driving and `_ui` folders. Neither diagnostic
failure is presented as a production defect or passing visual proof.

The Wave 2 package is now **revision 5**. Forty asset files change or are added;
four obsolete, unreferenced material/texture files are removed. These changes
are confined to the 405 and its menu model. Stable roster IDs, calibrated
script/physics recipes, other cars and save format remain unchanged. Unaffected
files are independently checked against the pre-change package inventory.

Linux's installed Wave 2 package was replaced with the verified revision, with
the previous installed directory retained for rollback. All 6 local profile
files remain byte-identical. Other installed packages were not replaced. The
usual launcher still points to the current native development executable; this
asset-only fix does not require rebuilding it or the Android APK.

Local combined bundle: `out/releases/OpenJuice-Car-Visual-Fixes-r6.zip`,
186,579,405 bytes, SHA-256
`19f380fe702e8789d4d0328851a75db7527e42385c6eebf6d5fc2de453679794`.
It contains 2,999 manifest-bound runtime files. Ferrari/Street Cars payloads are
unchanged from r5; Wave 2 is revision 5. Every ZIP payload was read back and
hashed. Authoring sources, model downloads, caches and user saves are excluded.
The first unpublished local ZIP accidentally included authoring tools; its hash
receipt is retained, the rejected payload was removed, and packaging now uses
the declared runtime manifest. It was never uploaded or installed on Android.

The RP5 remains untouched at the user's explicit request and still has v19 APK
and r5 car bundle. The r6 device import, post-import save comparison, physical
rendering and driving remain pending. No 60-FPS or Android certification claim.
The wider closure ledger, including the original strict 119-D8M failure and
remaining runtime/art coverage, remains open.
