# OpenJuice upstream 1.02.7 consolidated locally — 2026-11-02

Source: [Sidewinder's original OpenJuice repository](https://gitlab.com/sidecr3w/openjuice).
Code branch: `unofficial-patch/1.02/dx9`, the same patch family our 1.02.5
Linux/Android base followed. `master` is a branch index, not the engine.

- Previous source base: `v1.02.5`, commit
  `4e5ab27a` (the tag's full commit is in the backup receipt).
- Imported upstream: `v1.02.7`, commit
  `b23976cffcfdbd6553a3c406389f71f9097dfa00`.
- Pinned JuicedConfigExt submodule:
  `ec4aaab62cfeb71561e1848517e9a09625f5f1a0`.
- Local source checkpoint before importing:
  `8f479ea605d7c8af59d3e30ffe90885d558a12b1`, reachable through
  `backup/pre-upstream-1.02.7-20261102`.

## Integration method

Upstream rewrote/rebased its history, and the fork's imported source has no
shared Git ancestor. A normal unrelated-history merge would use the wrong
base. We fetched upstream and applied the binary-capable source delta from
the exact old tag to the new upstream commit with `git apply --3way --index`
in an isolated worktree. Eight textual conflicts were resolved by combining
the upstream changes with local behavior. No remote branch was overwritten.

Before changing source, all tracked and untracked nonignored work was saved
as a local commit and an independently usable Git bundle. The previous
Linux executable and file hashes were also saved. The import includes
upstream documentation, licensing notices, Windows projects, new shared
interfaces and third-party/submodule metadata as well as engine changes.

## Native adaptations

- Preserve SDL controllers, keyboard ownership, configurable touch/crew
  bindings and local controls editing. Upstream's Windows DirectInput wheel
  and pad readers are not instantiated on native ports. The platform-neutral
  layout/actions algorithms compile from the pinned submodule.
- Adapt the existing SDL mixer and FFmpeg stream decoder to upstream's new
  `Output`, `OutputBuffer`, listener and stream interfaces. Shared upstream
  sound-system and stream logic now run above this backend. Buffer duplication,
  PCM writes, streaming and end-of-file behavior have direct mixer tests.
- Preserve poker view creation and every local car/vinyl registration.
- Integrate upstream's loading/render turn scheduling. A waiting native frame
  services queued GL uploads on the context owner, including overlay-only
  frames, avoiding a loader/main-thread mutual wait. A new regression models
  exactly that dependency and checks which thread services the upload.
- Collect SDL text-input events for the new typed-text interface; retain the
  established game input paths. Shared numeric platform settings persist
  atomically outside profiles, with `JUICED_SUPPORT_DIR` isolation for tests.
- Update CMake/Nix versioning and native source lists for the reorganized
  upstream files. Windows crash-reporting and DirectInput-only tests remain
  Windows-specific; portable screen/layout/render tests are included.

## Validation

Evidence: `out/validation/upstream-1.02.7-20261102/` in the main repository,
with the external backup below retaining the complete logs.

- Complete native Linux CMake build under `nix-shell shell.nix` succeeded,
  including the gameplay executable and all configured test/diagnostic targets.
- Unit checks: **738 passed, 0 failed**.
- Native audio: **46 passed, 0 failed**, using the real SDL mix callback with
  an inaudible test endpoint, including the new output-interface checks.
- SDL controller test: menu navigation, driving/crew bindings, remapping,
  two pads, disconnect/reconnect and keyboard acquisition passed.
- CTest: **18/18 passed**, including core, audio, saves and local fake-server
  network regressions. No external multiplayer service is required.
- Hidden-window native launch: title → private copy of Tony's profile → main
  menu, 720 rendered frames, with AI Hero Liveries enabled.
- Track smoke: Peugeot 405 plus two other cars on
  `Angel Westside/AngelWest_R1`, 900 frames, beyond countdown into movement;
  sampled player speed reached 34.4 m/s. Captured with the actual renderer.
  This is a short driving check, not a full career or performance benchmark.
- All **315** AI vinyl/car combinations loaded in both garage/race, appeared
  in workshop menus and preserved their selected indices through isolated
  in-memory save round trips.
- Local `mods/` and Android launcher files remain byte-identical to the
  pre-import checkpoint. Actual profile hashes are compared before/after;
  no real save is used for runtime smoke tests.

The habitual Linux launcher is updated only after the isolated checks pass.
Its final build/profile smoke and hashes are in `deployment.json`.

## Limits

The new shared source is integrated; this is not a claim that the Direct3D
backend's new graphics are implemented by every native backend. The advanced
lighting/shadow/cloud/SMAA/heat-haze passes still need OpenGL/Metal equivalents.
Unsupported renderer operations retain upstream's fallback paths. Advanced
settings inherited from upstream can therefore have no effect in the native
renderer. The tested path is the default original graphics configuration.
The existing native SDL audio backend does not gain EAX from this import.

Android/macOS shared source signatures and build lists were adapted, but no
APK, macOS build or real Android-device validation is included. Published
release assets and remote branches were not changed. No new 60 FPS claim is
made. The full Nix derivation was not rebuilt in the Nix sandbox; the native
build and its CTest suite ran in the pinned Nix development environment.

## Backup and subsequent updates

External backup:
`/run/media/martin/Ediloco/OpenJuice-builds/20261102-upstream-1.02.7/`.
It contains `pre-integration.bundle`, `checkpoint.json`, original file hashes,
the tracked local patch, the exact upstream delta and previous executable.
Private profile copies used by smoke tests stay there and are not part of
source commits or distributable checkpoints.

For a later upstream sync, keep the **exact imported upstream commit** as
the comparison base even if its branch was rebased again. Repeat the isolated
three-way integration and native tests; do not replace our source tree with
an upstream archive or reset local changes. New checkouts must run
`git submodule update --init external/JuicedConfigExt` before building.
