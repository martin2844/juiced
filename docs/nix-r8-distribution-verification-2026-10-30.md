# Linux Nix r8 distribution verification — 2026-10-30

Local runtime archive:
`out/releases/OpenJuice-20261030-r8-linux-x86_64-nix.tar.xz`, **319,825,376 bytes**,
SHA-256 `da01487ad55f1773db2df182a95e5c518b4d7e20c32f34a400c2442b1bce3072`.
The adjacent checksum file and the archive's CONTENTS.sha256 bind its payloads.
This is a locally verified Nix-store distribution; it has not been publicly
uploaded. Original game data, runtime mod assets and saves are separate.

The package includes the visible tyre-envelope correction, preceding
split-render/results, poker correctness and Linux loading-queue fixes. Its
compiled source and tests are those documented in
`wheel-envelope-verification-2026-10-30.md`; no production code changes were
made during this distribution follow-up.

## Artifact/source binding

Actual output:
`/nix/store/vhsd6zahyljr2p5w3bpm3cd5wj10dh9f-openjuice-1.02.5-linux-mods1`.
ELF SHA-256 `d0bcbf2913acb18ea78935789c414e345a3eb30d564a8f204a6ea11c2180cd00`.
Derivation: `/nix/store/10yvlwi2ds95wmgfk3wvkk5zanjd7n1g-openjuice-1.02.5-linux-mods1.drv`.
All **905 critical code/build/tool inputs** match its actual source. Nix CTest
passes **9/9**. The existing actual-r8 900-frame Radeon/six-Ferrari sample reached
Go and exited zero; it uses installed mods and factory wheels. It is a bounded
runtime sample, not the 35,552 aftermarket-wheel geometry audit or a full race.
Historical r5 full-race/save/reload results are not relabeled as r8.

Exact source companion:
`out/releases/OpenJuice-20261030-r8-nix-source.tar.xz`, **5,730,612 bytes**,
SHA-256 `ab2f7b35f3bb27ee7ec9896fd0c21ab2d31026a0978c22d54e8cd58426da72a0`.
All **1,321 files** were independently extracted and hash-checked, restoring the
same source NAR identity and derivation. This is not a second binary rebuild.
Later documentary updates are retained in the checkpoint delta.

## Distribution validation

* Exported the complete **307-path** dependency closure, then imported it into
  a new, exclusively owned private Nix store. Dependency graph matches exactly;
  `--verify --check-contents` passes and imported ELF matches the exercised ELF.
* Ran the installer against an isolated profile under the validation directory;
  installed `--help` succeeds. The user's normal profile/launcher was not
  replaced. The installer chooses OPENJUICE_PROFILE or its dedicated state path.
* Created the compressed distribution, streamed it back independently and
  hash-checked **every payload**, including the NAR. Fifteen metadata payloads
  remain outside the archive for inspection after raw staging cleanup.
* Bundled Sidewinder/OpenJuice attribution, GitLab upstream link, required
  license notice, installation guides, exact compiled-source/build/import/
  runtime receipts and the 17-checkpoint historical source/asset chain.
* LOCAL-ARTIFACTS.json correctly identifies APK v23 and car bundle r6 as separate
  local candidates, with hashes/sizes. Neither is a new physical RP5 install.
* Removed only the verified raw export stage and private import-store tree after
  archive/source verification. Retained archives, source GC root, metadata,
  isolated test profile and receipts. Directory permissions were changed only
  inside the owned private test trees; real store file modes were not changed.

Evidence: `out/validation/linux-current-20261029/linux-package-r8`, especially
`final-verification-receipt.json` and `verify_final.py`. This verifier binds
current source, both archives, import graph/contents, runtime ELF, all metadata,
the historical checkpoint hashes, current companion artifacts and cleanup.

## Limits and next requirements

The runtime sample used the normal Nix store. Private-store import/content
verification does not assert independent gameplay from that store. This is a
Nix closure requiring Nix, not an AppImage or a physical Android test. The normal
Linux launcher still uses the current development executable, which differs in
build identity from the Nix ELF while matching the corrected source.

Full original-data integration remains **260 passed / 1 failed** for the strict
119-D8M inventory. Original caller/particle follow-up is documented in
`legacy-mesh-audit-2026-10-29.md`; no guessed legacy reader or exemptions were
added. Broader campaign/route/network/art coverage and final source/license/
publication review remain open. RP5 remains untouched on APK v19 / car pack r5
at the user's explicit direction; current local APK v23 / pack r6 still require
device deployment, save comparison and physical visual/driving review.

This pass is retained in
`out/releases/OpenJuice-verification-checkpoint-20261030-r8-resource-audit.zip`,
based on the immutable wheel-envelope checkpoint. The receipt/checksum bind all
source-document/evidence payloads. No private user profiles are included.
