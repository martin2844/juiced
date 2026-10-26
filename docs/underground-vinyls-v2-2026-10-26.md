# Underground vinyl collection: visual refinement checkpoint

Date: 2026-10-26. Supersedes the rough visual conversion in commit 590a35a.

## Result

All three source collections remain represented: 252 recovered art layers become
241 selectable options on 21 cars (219 ND4, 15 FNF, 7 AJ). Eight known designs
combine 19 source layers. Independent hood, stripe and badge alternatives remain
selectable. A Skyline opacity mask is no longer offered as artwork. RX7 Sun is
assigned to RX7 rather than S2000.

The converter now interprets RGB tint hues, with explicit palettes for key F&F,
rally and other designs. These are documented adaptation choices, not recovered
player settings. Partial side artwork is trimmed and fitted proportionally on
both sides. Broad wraps retain their layout. Panda Trueno has deliberate trim,
bonnet and original Fujiwara lettering placement. Workshop thumbnails are actual
native car captures, with a bonnet view for bonnet-focused options.

Textures retain useful source resolution and include full filtered mip chains.
Conservative alpha-based culling reduces aggregate garage **decal** geometry from
441,692 to 189,741 triangles (57%). This is neither whole-car geometry reduction
nor a measured FPS gain.

## Verification and evidence

- Source conservation check: all 252 original art IDs occur exactly once in the
  output or its composite metadata. Also checks nonempty alpha, car assignment,
  mask exclusion and complete native texture mip data.
- Native `android_vinyl_check`: all 241 options pass garage/race mesh, material,
  texture, workshop membership and in-memory career save/read checks.
- Three native views captured per option. Full contact-sheet review followed by
  explicit palette/layout fixes and final five-design visual review, including
  updated workshop thumbnails.
- Host test rebuilt successfully; optional capture paint argument is diagnostic
  only. Rendering uses desktop OpenGL, not an Android device performance test.
- ZIP integrity check passed. Third-party artwork stays in ignored local output;
  source credits, archive hashes and individual readmes accompany the package.

Local deliverables, relative to this repository:

- `out/releases/underground-vinyls-v2/OpenJuice-Underground-Vinyls-v2.zip`
- `out/releases/underground-vinyls-v2/gallery/index.html` — all 241 options.
- `out/releases/underground-vinyls-v2/featured.png` — five selected designs,
  three angles and workshop views, with illustrative base paint colours.
- `out/releases/underground-vinyls-v2/native-validation.log`

Reproduction instructions are in `tools/vinyl_pack/README.md`; palette and
composition recipes are in `tools/vinyl_pack/curation.json`.

## Scope and installation status

The supported visual target is stock Juiced bodywork. Some full-body wraps still
retain source-car contour differences. Custom kits, damage, runtime recolouring
and on-device performance are unverified. The art transfers do not add missing
car models, and are not pixel-identical NFS reproductions.

The package requires the native catalogue extension available in the local
Android `.9-vinyl-preview` build; existing public binaries may lack it. This
iteration changes assets and conversion tools, not production Android code.
Package starts disabled. Nothing was installed onto NixOS or Retroid, no real
saves were modified, and no third-party art was publicly uploaded. Enabling asset
mods changes the catalogue save variant; do not copy Custom Logo careers between
variants without remapping the appended decal slot.
