# Nix r10: packaged Career save correction — 2026-10-31

The corrected Career respect/history behavior is now in a clean, verified
Linux/Nix distribution candidate, extending the
[development/Android save correction](post-race-save-verification-2026-10-31.md).
No production source changed in this packaging pass. The habitual launcher
still uses the corrected development ELF. No RP5 interaction or public upload
occurred; physical RP5 remains APK19/car bundle r5.

## Build and actual runtime

`nix-build --cores 4 --max-jobs 1 default.nix -o out/linux-r10-result`
completed a clean build and **10/10 CTest**. The actual compiled source is
`/nix/store/64pqzsxyrzlcdrvpfshja2qvgbb8ii7w-source`; its **907 critical files**
match the workspace, including all eight inputs of the preceding save fix.
Exactly those eight critical inputs differ from r9. Seven installed notices
match their source files. FFmpeg dependencies remain the same LGPL-configured
libraries verified for r9; no media decoder change or new media equivalence
measurement is implied.

Actual store output:
`/nix/store/2d61w7jgvhp2wgi15dyxkf39qmm4y3m8-openjuice-1.02.5-linux-mods1`.
Unwrapped ELF SHA-256:
`caa9f91a625ae3c8a4bc57b45f2e109bd99bef183db495c558875cebcc98db0a`.
The live process's `/proc/PID/exe` was checked against this exact file, rather
than identifying the shell wrapper as the running game.

That packaged executable follows the normal title/driver/load/Career/calendar/
event setup/grid/race/pause/abandon/results/skip-all/autosave/workshop/save-exit
flow with the installed mods on Radeon. It renders **2,111 frames** and exits
zero. The owned synthetic 406 event has zero fee, prize and titles; it is a
normal last-place abandonment, not a completed lap, victory or natural campaign
progression. Controls/readbacks use the existing process-local SDL harness,
without game-state writes.

An independent archive decoder verifies that the first results autosave already
has **584 respect**, **584 in the new history sample**, history position **2**
(one point added), and **985,400 cash**. The subsequent Career save-and-exit
archive is byte-identical in full. The bought kit/carbon hood/retail vinyl8/wheel47
configuration remains present. This is a separate actual Nix-ELF reproduction;
the prior development suite and its original-code negative controls are retained
in the preceding checkpoint, not relabeled as Nix-produced test executables.

All eight actual readbacks were inspected. Loaded Career, calendar, abandonment
confirmation and final respect screen are usable visual evidence. Four captures
were too early: results before its text, a black workshop transition, car choice
before its car preview and the outgoing title-menu transition. Those are marked
excluded in `runtime-receipt.json` and `gallery.html`; they do not approve car
appearance or settled UI rendering. No runtime was repeated merely to replace
these captures. The final respect readback visibly shows 584.

## Distribution and source restoration

All **307 closure paths** were exported and imported into a new owned test store.
Its dependency graph matches, `--verify --check-contents` passes and the imported
ELF hash matches. A dedicated `out/.../install-profile` installation and its
`juiced --help` pass. Runtime testing uses the normal Nix store, not that private
import root; the user's launcher/profile was not replaced.

The actual filtered engine source is archived separately. All **1,336 files**
were independently extracted and hash-checked; restored source NAR identity and
Nix derivation match the actual input. This is a source/build-definition
reproduction, not a second independent compiler run. The unchanged exact FFmpeg
source/patch/configuration companion from r9 is referenced rather than duplicated.

| Local artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `OpenJuice-20261031-r10-linux-x86_64-nix.tar.xz` | 319699356 | `81123b944781981e49c7e1bf2ee75b92786b8e513672efe7c6db668c5ebc2593` |
| `OpenJuice-20261031-r10-nix-source.tar.xz` | 5759004 | `e950d4d2191dea53a7021b42538f2ad9d4f3600526114509171efbef395710b0` |

The runtime tar's **21 payloads** were read back and checked. It contains the
Nix closure, isolated-profile installer, upstream attribution/license, component
notices, exact build/source/runtime receipts, the preceding **23-checkpoint**
chain and hashes for the matching source, unchanged FFmpeg source, APK24 and
car bundle r6. Original game data, runtime mod assets and user saves remain
separate. The signed APK24 and installed Wave2 files retain their hashes.

After successful archive/source/import checks, only the owned raw distribution
stage and private test-store tree were removed: **2,210,838,765 logical file
bytes**. This is not a physical-space reclamation guarantee. The real Nix store,
source GC root, dedicated installed profile, archives and evidence remain.
Six personal Linux save files retain their previous hashes.

## Scope and remaining work

This closes the pending Nix packaging of the post-race save correction. It does
not close the whole verification goal. Full development integration retains
**371 passing checks / one failing check**, including the strict original
**119-D8M** parse gate. Broader car art, campaign/routes/network, physical
Android testing and final publication review remain open. No FPS or complete
distribution legal approval is claimed.

Evidence: `out/validation/linux-current-20261029/linux-package-r10/`, including
`final-verification-receipt.json`, exact source/hash maps, build log, actual
process/capture bindings, archive/import/installer receipts and cleanup proof.
Actual UI actions/logs/captures are under `full-race-ui/career-results-nix-r10`.

```sh
python3 out/validation/linux-current-20261029/linux-package-r10/verify_final.py
```
