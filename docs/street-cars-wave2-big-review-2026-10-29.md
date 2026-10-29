# Street Cars Wave 2 — big review, 2026-10-29

Scope: the seven-car pack, its Blender/native authoring pipeline, required
first-wave importer/package dependencies, 64-model save capacity and runtime
consumers. Existing poker, crew-racing, Ferrari handling and unrelated Android
UI edits are outside this commit. No user saves or installed packages were edited.

## Findings fixed

1. **High — removing a spoiler removed bodywork.** The position heuristic
   classified10,656Jetta trunk/body/rear-door triangles as a detachable spoiler.
   Wave2 now keeps that bodywork fixed in CHASS and does not advertise a synthetic
   spoiler. The new native regression failed28checks on the old assets and passes
   after regeneration, including geometry preservation with no spoiler selected.
2. **High — save editor rejected roster migration.** Career could read an old
   added-car extent but the editor's exact round-trip guard compared it with an
   expanded extent. The editor now validates the original serialized extent,
   while retaining initialized progress for newly added cars. Stock/old/full
   extent migrations and corruption guards are exercised in the host tests.
3. **High — arcade selection omitted the final three cars.** Fixed60-slot groups
   silently lost IDs61–63 in the final group. Groups expand with the model roster;
   stock retains60slots. Selection, wraparound, network selection and opponent
   generation use actual group width. Tests cover every added ID and stock behavior.
4. **Medium — exported kits lost modifier geometry.** Blender previews included
   solidify/bevel, but GLB export did not. Export now applies modifiers and checks
   actual exported diffuser thickness. Negative test rejects the old flat fins.
5. **Medium — failed captures could report old evidence.** The screenshot command
   ignored nonzero exits and reused old BMPs. Captures now discard prior evidence,
   require success/new output and atomically replace the PNG. A failing executable
   regression verifies old screenshots cannot survive as current evidence.
6. **Medium — reproduction depended on a transient launcher cache.** Rebuild now
   accepts explicit game/base/work/Blender paths and validates prerequisites.
   Staging rejects directory symlinks that could redirect writes into inputs;
   stale generated paths are removed. Packaging validates inputs, writes a fresh
   directory atomically and includes shared exhaust asset directories when present.
   Archive destinations inside the package are rejected so a ZIP cannot include
   its own temporary output; this has a dedicated negative regression.

## Evidence and limits

Native candidate after spoiler/kit fixes:1,217passes, zero failures. Pipeline
failure tests:6passes. Exact final host/build/assembled-package results are recorded
below when completed. Geometry snapshots and device performance are distinct:
functional checks do not certify optical fidelity, authentic handling or60FPS.

Existing-save mutation checks remain enabled. A possible cross-profile tail leak
was investigated and not reported: production load paths initialize a fresh
career/EditorGame before reading; resetting the tail would instead erase intended
unlock defaults. No speculative finding was promoted.

Generated meshes/textures are runtime deliverables. No lint/style findings were
raised. Wave2 original downloads and Blender working files remain outside Git; creators
and source/license metadata accompany both runtime packs. Android driving tests
remain pending because RP5 is not connected.

## Final verification

- Clean snapshot of the selected commit content built successfully with Nix/CMake.
  Unrelated dirty workspace features were absent from this build.
- Clean unit executable: **259 passed, zero failed**.
- Native package host assembled Ferrari + Street Cars + Wave2 from the clean
  snapshot. Its freshly built checker: **1,224 passed, zero failed**, including
  all seven added IDs in the final arcade selection group.
- Android host editor integration: stock→11 and4→11 migration, preservation of
  initialized unlock defaults/unrelated archive entries, corrupt input rejection,
  all roster cars with positive power: PASS.
- Focused arcade suites:69+3passed, zero failures (also covered by clean units).
- Python staging/capture/package failure regressions: **6 passed**.
- Blender export negative/positive check: flat fins rejected, applied solid fins
  accepted. The generated assets were rebuilt after the fixes.

Logs and source-generation evidence remain under
`out/street-cars-wave2-20261029/review-*.log`. Runtime assets are committed under
`mods/street-cars-wave2`; required first-wave assets/importer are committed under
`mods/street-cars`. The corrected local ZIP is
`out/street-cars-wave2-20261029/street-cars-wave2-reviewed.zip` and also replaces
the old `street-cars-wave2-private.zip` link target, retaining a backup.

No confirmed findings remain open in this review. Factory gearing approximations,
visual polish beyond the reviewed defects, and RP5 driving/performance approval
remain explicitly documented limitations, not claims established by these tests.
