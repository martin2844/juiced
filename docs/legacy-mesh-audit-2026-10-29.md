# Original-model audit follow-up — 29 October 2026

The strict inventory gate still reports 119 rejected D8M files: 115 structurally
old-layout candidates and four empty files. This follow-up does not change the
parser, suppress the failure or declare every game resource verified.

The current native car and frontend workflows already resolve release-layout
meshes in their own folders. Supporting a guessed additional format solely to
make the inventory green would not establish original-engine behavior.

## Original executable evidence

Selected artifact: `/home/martin/Projects/juiced/game-data/Juiced.exe`, x86 PE,
SHA256 `6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b`.
REA/Ghidra 12.1.2 inspected the shipped file without executing or modifying it.
The session is closed and its evidence bundle and snapshot are retained under
`out/validation/linux-current-20261029/legacy-followup`.

Repository comments name addresses from another retail build. A request at
0x5582B0 resolves to the selected artifact's enclosing constructor 0x557DD0;
0x558510 resolves to a stream constructor at 0x5583D0. These are not verified
entry addresses for the source-commented functions. Raw objdump disassembly is
retained independently of the decompiler's recovered signatures.

At 0x557EFA..0x557F01, the file constructor reads four bytes into a stack value.
At 0x557F0A it loads that value into EDI, doubles it at 0x557F1F and calls index
creation at 0x557F22 / 0x51CF50. The next four-byte read at 0x557F2D..0x557F33
is the part count. The stream constructor has the same initial read/double/index
creation pattern at 0x55840E..0x558455. Its observed part reads match material
name, index range, face type, bounds and later vertex data of the release layout.
These observations support the existing reader's initial release-layout
interpretation; they do not establish every original loader/caller path.

REA Evidence IDs:

- `ev_1e9f806731c94d60e1a7b3edecdd1169fcbfcb1d25cb4fb3a618c61540c3b589`:
  file constructor observation, enclosing procedure 0x557DD0.
- `ev_fa51fcead68b5fe3b665f39c58a2e85517e3ad3267b1fb90b83e10e1a13bc7f1`:
  stream constructor observation, 0x5583D0.
- `ev_ebda290a24281e2afc7b249811a87d9aea1abd8101dc45084348d2396735deef`:
  index-job construction, 0x51CF50.
- `ev_ed2340c6891ae3c9ad18e1d74fc444083ad65a4c09556beb7f20781313ea6584`:
  renderer job queue, 0x51C830.
- `ev_acaee8e0b916560cc5ca57df09cea5b42189ded0f36a52d342026edbe401d51b`:
  16 observed bytes of job vtable 0x6F73C4.
- `ev_6ec7e2bfac024162485646e4a18c6a6562ad5b73228797712331766351849d84`:
  116 observed bytes at its index-job method 0x51C230.

Ghidra did not recognize 0x51C230 as a procedure, so there is no decompilation
claim for that method; its tool error and raw byte/disassembly evidence remain
separate. Ghidra also reports terminal-flow/function-boundary and calling
convention uncertainties in the constructors. Their pseudocode contains
recovered variables and is not original source or execution evidence. The
bundle's machine unknown count does not discharge these explicit limitations.

## Projection over all rejected assets

`original-header-projection.json` binds each rejected file's contents by hash.
Projecting the observed first-u32 interpretation and 32-bit doubled index-byte
argument onto the actual files gives:

- 111 nonempty files request an index payload larger than their whole contents.
- Four have first word zero but a next word larger than 65,536; those words are
  spatial floats in their candidate old layout, not a plausible release part count.
- Four are completely empty and cannot supply the first read.

This is a static projection, not observed original-game rejection or a D3D
allocation test. It does not prove that no converter or other caller handles
an old layout elsewhere. The old-format classifier's earlier truncation,
trailing-byte, oversized-count, invalid-index and part-range negative controls
remain in `legacy-mesh-negatives.json`.

## Current-port caller boundary

The prior resource audit contains 18 unresolved references across **eight**
global BOHs: celica_stock/crx_stock/golf_stock/gtr_stock/rx7_stock (three each),
rx7_fbump_vs1/rx7_fbump_vs2 (one each), and garage (one).

A separate read-only source review found no selecting entry point in current
native loadModel/addModel callers or the audited retail UI/script resources.
The generic loader reads exactly the current model-folder/name. All eight
SetModel properties in 2,394 UI and 330 script archive entries name Angel_City.
Other audited callers use fixed garage/rolling-road/shadow names, fixed territory
names or the UI_<route>_GUI/MAP families. Current car stock trees use
cars/<car>[_ui]/models/<car>_stock.boh. The workshop uses don_garage1 rather than
the separate global garage.boh.

Source anchors: `Scene3d.cpp` loadModel and View3d SetModel; `CarModel.cpp`
loadModel and its local-first mesh lookup; `CarView.cpp` workshop/garage names;
`CityMap.cpp`, `RaceHud.cpp`, `Respect.cpp` generated names. Exact prior corpus
hashes and paths are in model-reference-audit.json and global-model-ui-audit.json.

No currently reachable failing selection was established in that bounded
source/resource review. Original-executable whole-program reachability, DLH
paths and arbitrary external/generated inputs remain unknown. An unused-file
inference is not a proof of global unreachability. The 119-file strict gate stays
visible; next playability checks exercise actual loaded scenes and races rather
than introducing guessed parser semantics.

## DLH direct-reference follow-up — 30 October 2026

A native out-only audit now reads all 132 original scene-object files through
GameFiles and parseDlh: 3,086 objects, 8,082 nodes, 553 inline meshes, 1,026
inline submeshes, 48,807 vertices and 73,823 indices. Independent archive reads
bind every file by SHA-256/size. Truncating each of the 132 payloads is rejected.
All 31 effectRef records are empty.

Current TrackScene directly constructs meshes from these embedded index and
vertex buffers. SceneObjects uses object labels to identify emitter definitions,
not as model filenames; effectRef is not used to select an external model.
The other direct file lookups in SceneObjects are dynamics scripts and OCD
collision hulls. Thus the audited direct DLH geometry path does not select the
legacy global D8Ms. Effect-library indirect resources and original-executable
whole-program paths are separate questions; neither is declared proven here.

Evidence: `out/validation/linux-current-20261029/scene-object-reference-audit`,
including native source/build commands, per-file JSONL, independent hashes and
receipt. No production parser/resource/save was changed. The complete current
integration run after the separate poker fixes remains 260 passed / 1 failed,
with the same strict 119-resource inventory failure visible.

## Original caller and particle-material follow-up — 30 October 2026

Reopened the same SHA-256-bound executable with the retained REA snapshot,
reusing the four prior constructor/job dossiers. Fourteen additional native
function dossiers extend the original call graph; all 18 dossiers and two byte
observations are retained in `legacy-loader-callers/rea-evidence.json` and its
new session snapshot. The original artifact and production parser retain their
existing hashes. The REA session was closed after export.

Observed original file path: `0x554610` selects wrappers `0x557AC0` or
`0x558F90`; both reach the already inspected release-layout file constructor
`0x557DD0`. The stream constructor `0x5583D0` is reached through wrapper
`0x559030`, called by `0x54E500`. Recursive BOH node loading at `0x534980`
uses that same file factory; its parent `0x534D40` appends `.boh`.

The original garage caller at `0x436490` explicitly selects `don_garage1`.
Dealer caller `0x438260` names `showroom2`, `showroom3`, `scrapyard` and
`don_garage1`. Car model callers construct a `models` subfolder and `%s_ui`
variant or use the model name from car data. This corroborates the previously
audited source paths, rather than establishing that a bare `garage.boh` is used.
The generic view/model holder remains parameterized; dynamically supplied names
and indirect calls are not globally proven unreachable. Ghidra's recovered
ABI/stack values, terminal-flow warnings and entry boundaries remain explicit
limitations. Static references are not runtime observations.

Selected REA evidence:

* Factory `0x554610`:
  `ev_4891985d5a18ccac1a480f49fceaad87ee63e3f984ab01048d4b423737506c75`.
* Recursive node loader `0x534980`:
  `ev_6343b4ded01aaa50a07f64f9843745df983bdce63ceb52fff26e3635ff53a95d`.
* Garage selection `0x436490`:
  `ev_875398190830f7839228ce6b50085da6eaeb5787ab6f211486c130b948423397`.
* Dealer selection `0x438260`:
  `ev_904a46442f7f158c8039141a2e5927b80600c8843f272d7675f677a4db869839`.

`record_callers.py` verifies the retained caller-chain edges, protects the
original executable/parser identities, and records raw objdump slices for the
factory and garage selection independently of recovered pseudocode.

The separate current-source particle path now has a bounded retail resource
audit. EffectDef contains a material name and particle parameters, not a D8M
filename. ParticleSystem resolves it to `mesh/mats/<name>.pmd`, loads declared
textures and builds particle geometry procedurally. TrackView loads the common
and one weather-specific definitions plus surfaceeffects; DLH effect labels
select emitter definitions rather than model files.

The common/dry/wet/surface corpus contains **34 unique effect definitions**,
**13 material PMDs**, **13 explicit DDT texture references**, and **30 non-RUSH
surface-effect references**. All hashes/sizes and references resolve. No path in
this audited chain selects a global D8M. `legacy-effects-followup` retains the
original corpus, byte-identical independently generated receipt, extractor,
verifier and source paths. This closes that particular source/resource gap;
it does not prove original-engine execution or whole-program unreachability.

No guessed legacy parser, zero-length-mesh acceptance or inventory exclusions
were added. All 119 original rejections remain visible in the strict integration
gate; original whole-program/runtime reachability is still unresolved.

## File-factory caller extension — 31 October 2026

Four new dossiers extend the original set to22, using the same original PE and
retained snapshot. Direct file-factory calls and the stream-dispatch case1 are
independently checked in objdump; referenced ASCII literals are bound through
independently mapped original PE sections. All119 rejected original payloads
are reread/hash-matched; no parser or inventory change. Dynamic producers,
other dispatch branches and original whole-program/runtime reachability remain
open. The new REA session was exported and closed. See
`legacy-factory-callers-verification-2026-10-31.md` for Evidence IDs, exact scope
and retained data. No119-file gate pass is claimed.
