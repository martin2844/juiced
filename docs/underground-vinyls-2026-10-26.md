# Three-pack vinyl conversion checkpoint — 2026-10-26

Scope: the user corrected the initial four F&F designs to **all recoverable vinyls
from the three packs**. This checkpoint is a development preview, not approval of
every design as a finished livery.

## Delivered locally

Under `out/releases/underground-vinyls-preview1/`:

- `OpenJuice-Underground-Vinyls-preview1.zip`: optional asset package, disabled by
  default, with source provenance, hashes and original author readmes.
- `OpenJuice-1.02.5-android.9-vinyl-preview-arm64.apk`: signed ARM64 build containing
  the new catalogue loader, versionCode 10. Build succeeded; not installed or
  tested on the Retroid during this task.
- `gallery/index.html`: searchable local gallery, three native garage views per
  option; it does not use mockups or generated car imagery.
- Native validation, negative registry checks and Android build logs.

253 unique selectable artwork layers across 21 cars: 229 first attributed to the
ND4SPDWorld collection, 15 to the Haris Ahmad collection, 9 to AJ_Lethal. Identical
decoded artwork across sources is represented once, with duplicate provenance in
the catalogue. These counts are **not 253 complete original liveries**. Some NFS
designs consist of multiple separately selectable layers, including hood-only
graphics and sponsor badges. Sixteen excluded colour candidates are recorded in
the source inventory (non-vinyl body/wheel/light textures and a companion mask).

The complete local source inventory and originals remain under
`out/vinyl-research/`. Conversion tools and source URLs are documented in
[`tools/vinyl_pack/README.md`](../tools/vinyl_pack/README.md).

## Implemented

- Recover nested archives without running bundled executables. `unrar` recovered
  DDS files that libarchive had extracted with CRC errors. p7zip incorrectly saw
  an embedded ZIP as the outer RAR in initial attempts; it is unsuitable here.
- DDS decode, mask pairing, decoded-image deduplication, provenance and explicit
  target-car assignments. Retarget absent 240SX art to 300ZX and Tiburon to Celica;
  otherwise unspecified vehicles use Golf, recorded as adaptations.
- Build native DDT textures, PMD materials and D8M paint-surface meshes for stock
  garage and race models. Apply BOH part transforms before projection; the first
  prototype lacked these and produced detached bonnet/bumper artwork.
- Distinguish probable tint-channel alpha from actual full-colour transparency.
  Use the external mask for channel maps, retaining overlapping source alpha for
  full-colour graphics. This corrected both invisible stripe artwork and broad
  black backgrounds on AJ_Lethal artwork. The classifier remains heuristic.
- Native additive `mod_vinyls.txt` catalogue, bounded decal count, mesh-name and
  target validation, duplicate rejection. Retail decal entries remain first.
- Workshop thumbnails, short captions and full help text. Long captions were
  tested and replaced because they overlapped in the stock horizontal selector.

## Evidence

Final candidate: `out/vinyl-research/candidate2/`.

`android_vinyl_check` reports **253 passed**. It loads both native model variants,
requires decal geometry/material/texture, writes and reads an in-memory career,
and verifies workshop menu membership and captions. It captured both sides and
a front three-quarter view for every option plus a menu for each supported car.

`check_registry.py` passed absent-registry baseline, unknown target car, path
traversal, duplicate entry and capacity-overflow cases against the real loader.
The baseline reaches "No imported vinyls" after successfully loading retail data.

The existing mod-garage asset regression also passed with the vinyl package in
the assembled overlay: 202 wheel/blur sizes, 34 Ferrari decals, carbon material
and stock power, in garage and race variants. This test reported creative Lua mods
disabled; it does not establish Custom Logo runtime or save-migration compatibility.

Side contact sheets were inspected across the full catalogue; three-angle Supra
captures were additionally inspected after the transparency fix. This is not
manual approval of every pixel, seam or every camera angle. Native load/save tests
do not establish visual fidelity, crash-free races or Android performance.

## Remaining work before a finished release

1. Compose the multi-layer designs according to their readmes instead of exposing
   every component as a separate option. Preserve the recoverable individual art.
2. Review and explicitly author colour palettes and per-design UV placement. The
   automatic five-zone projection is approximate and some designs clip, stretch,
   sit at different heights on the two sides or retain NFS colour channels.
3. Validate custom body parts and damage. Current meshes follow stock bodywork;
   alternate kits are not supported by the conversion yet.
4. Run real workshop purchases and races on Android, measure loading/memory and
   verify catalogue/save-variant behaviour with the existing mod combination.
5. Do not migrate existing Custom Logo saves by copying bytes: its appended index
   changes when this catalogue is enabled. A proper remapper is required.

No installed mods, original archives, Retroid data or existing saves were changed.
The package starts disabled. Enabling an asset mod selects a separate catalogue
save variant; disabling it restores the old combination. No public release or
upload of third-party artwork was made.
