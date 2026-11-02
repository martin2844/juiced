# Source restoration and packaged Linux runtime — 29 October 2026

The dated source checkpoints restore the current engine changes successfully.
A fresh isolated Nix build from that restored checkout passes CTest **9/9**.
Its installed, wrapped executable also loads a copied validation profile,
renders menus and a challenge, reaches matching results and returns to Arcade.
The saved archive remains byte-identical after that run.

This verifies source recovery and a working packaged native build. It does not
declare the whole game complete or certify the physical RP5.

## Restored source state

Restoration uses base commit `98c929ce0ac56eff029c7a58ecc816380d65bf67` and, in
order, the render-storage source checkpoint plus the races-network,
arcade-overlay and challenges supplements. Their identities are recorded in
`source-reproduction/restore-receipt.json` and the package's
`SOURCE-CHECKPOINTS.json`.

The base checkout is an exclusively owned local clone. The binary patch is
checked before application; untracked files and later source supplements are
restored from their archives. No existing build output or object file is copied
into the new build. The main checkout's Git index and status remain unchanged.

The independent full source inventory finds 5,977 byte-identical files, 276
correctly deleted files, no missing file and no extra file. Three otherwise
unchanged Visual Studio project files are checked out as CRLF by
`.gitattributes`; the existing workspace has their normalized LF Git blobs.
All three match their base Git blobs after that declared checkout conversion.
Every C++ source, header and CMake file matches exactly.

The initial strict byte comparison exits 1 and records those three differences.
It is retained rather than relabeled as an exact-byte pass. A separate verifier
accepts only those project paths with the declared `eol: crlf` attribute and an
unchanged Git blob. Nine negative controls reject changed content, an incorrect
attribute or applying that allowance to a production C++ file.

Evidence scripts: `restore_source_checkpoint.py` and
`verify_restored_source.py` under `out/validation/linux-current-20261029`.
The parity receipt describes the checkpoint state before this new report was
written; this report and its ledger addition are documentation supplements.

## Fresh Nix build and installed runtime

From the restored repository:

```sh
nix-build default.nix --out-link ../linux-package --cores 4 --max-jobs 1
```

The pinned derivation builds in Nix's isolated build directory, runs all nine
data-free CTest suites successfully and installs the wrapped executable.

- Store path: `/nix/store/sl0d79724qiz5s0319wqlg1llcl8q1v2-openjuice-1.02.5-linux-mods1`
- Actual ELF SHA256: `36ff27c5d5b5fb89eaf2203df5ae40249dc832733dfdbab9c96f8678285f6257`
- Wrapper SHA256: `6dd1d7b68ab7799d47481742d4efe07aa569b6d744b9754dcfe43a18dc0ee2b9`

The running process's `/proc/PID/exe` was checked against that installed ELF.
The development executable's earlier `e9f493...` identity is separate: a Nix
package has its own compiler environment, fixup, wrapper and stripping. No
byte-identical comparison to that development binary is claimed, and a second
independent bit-reproducibility rebuild was not performed.

The runtime uses the documented event-order fixture from the challenge
checkpoint. It loads the one-win validation profile, displays its completed
Perfect Lap slot, selects the original Beetle/Downtown_R2_REV Speed event and
stays neutral without autopilot. Actual Go, zero applied throttle and finish at
10.01 seconds lead to `TARGET SPEED NOT REACHED`. Quit confirmation unloads the
race and returns to Arcade. Seven missing-evidence controls are rejected.
The game and private Xvfb display both exit zero; no preexisting user profile is
loaded or changed.

An early Down was injected while the score-loading modal was active, leading
into Career instead. That path was left normally through its quit question,
without a save. Its black transition capture, misleadingly named
`package-arcade-loaded.png`, is excluded as Arcade evidence. The recovered
Arcade selection and the actual challenge are captured separately.

Rendering is llvmpipe with a virtual SDL gamepad and dummy audio. This is not a
physical-controller, hardware/FPS or audible-mix test. Source parity supports
the relationship to prior checks, but does not turn earlier runtime evidence
into a fresh run of every car, route or Career branch on this package.

## Dependency export and installation

The full 307-path runtime dependency graph is exported to a Nix archive and
imported into a previously empty, exclusively owned local Nix store. The imported
dependency graph matches exactly; `nix-store --verify --check-contents` exits
zero and its imported ELF matches the package SHA256. Runtime execution above
uses the normal store on this same host, not a second operating system.

The installer is exercised against a dedicated validation profile and its
installed `juiced --help` succeeds. No default user profile, desktop launcher,
game data or saves are replaced. The distribution is a Nix closure for x86_64
Linux/NixOS; it requires Nix and compatible graphics drivers. Game data and mods
remain separate. The upstream required copyright notice and license accompany
the package.

Evidence under `out/validation/linux-current-20261029/source-reproduction`:
`nix-build.log`, `nix-build-receipt.json`, `nix-runtime-receipt.json`,
`closure-import-receipt.json`, `closure-contents-verify.log`,
`install-test.log` and `installed-help.log`. Actual runtime logs/captures are
in `full-race-ui/nix-package-challenge`. `verify_nix_package_runtime.py` checks
the packaged run independently; `package_verified_nix.py` packages and reads
back every archive payload.

The strict original-data inventory failure (119 D8Ms) remains visible and is
separate from the data-free CTest suites. Broader Career/team/pink-slip,
route/customization and physical-device coverage remain in the global ledger.
The current r5 car ZIP and v17 Android APK are unchanged. No RP5 is present in
ADB or the kernel USB inventory at the end of this check.

## Local delivery and temporary cleanup

Archive: `out/releases/OpenJuice-20261029-linux-x86_64-nix.tar.xz`.
Size: 319,172,360 bytes. SHA256:
`645279568816f30d34ae4de9d28a1c814d61a54979ff5eee4c6c9f709a28da30`.
Every archive payload was read back and matched against the staging hashes.
The archive includes installation instructions, license/notice, source-checkpoint
identities, build/runtime/import receipts and the full Nix dependency export.
It is a local candidate; no new GitHub publication is claimed.

After verification, only this run's restored working checkout, isolated import
store and raw package staging are removed. Their proof receipts and metadata,
the compressed delivery, its checksum and the installed Nix package remain.
`owned-temporary-cleanup.json` records the exact exclusively owned targets and
bytes reclaimed. No original source, game data, mod installation or user save is
removed. Rechecking source restoration/import requires recreating their owned
fixtures; the runtime/save receipts and installed package remain inspectable.
