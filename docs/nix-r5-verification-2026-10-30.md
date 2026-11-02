# Nix r5 corrected Linux package — 2026-10-30

This local candidate includes the Linux owner-queue loading correction. It does
not change the Android APK, installed car packs or normal development launcher.
No RP5 interaction, Git commit, push or public release is part of this pass.

The single Nix build completed with **9/9 CTest groups**. Its output is
`/nix/store/hvbkyp4cfcbj3nqgiw3a184ra57y4256-openjuice-1.02.5-linux-mods1`.

- Actual ELF SHA-256:
  `68d4fa9ec6b1a7838d90e2659ec470326877518c13015528cb934cef1510adb8`.
- Wrapper SHA-256:
  `9f8a749fab6db6122b9087780de63a9b1f1a6d7eba6fe87bf4b25226c1d6cd2c`.
- Derivation:
  `/nix/store/vn2s8phmpjhbkn5d3bsw29k52c14v73r-openjuice-1.02.5-linux-mods1.drv`.
- Actual compiled input:
  `/nix/store/4y4hcc5fl1mapzv34z5qaya756811c4d-source`.

All **904 checked code/build/tool inputs** match the current worktree. Later
documentary updates are separately checkpointed. The compiled source is retained
through an owned indirect GC root, and independently restored from the exact
source companion archive. All 1,317 file hashes, NAR identity and resulting
derivation match. This is source/build-definition restoration, not a second
independent binary rebuild.

Source companion:
`out/releases/OpenJuice-20261030-r5-nix-source.tar.xz`, 5,720,480 bytes,
SHA-256 `ae15bfea5f81efbe424ecfa24912fdc21a8667a97941c80b06c962de3baf5485`.
Original game data, runtime mod packs and user saves remain separate.

The actual wrapper/ELF loads naturally earned Arcade progress, completes the
four-lap Novice final first at209.14s, performs the real save-device autosave,
and separately reloads the complete series. Full archive/payload comparisons,
bonus flags and eleven missing-phase controls pass. See
`novice-series-verification-2026-10-30.md`. These are owned synthetic profiles,
ordinary frontend/input and diagnostic autopilot; not physical-input/FPS/audio
certification or a full-game completion claim.

All **307 closure paths** export and import into an empty owned local Nix store.
Its dependency graph and complete contents check match, including the exercised
ELF hash. Actual gameplay runs in the normal store; private import proves
content integrity, not a second isolated gameplay run. The dedicated test-profile
installer and installed `--help` pass. User profiles/settings/launcher are not
replaced. The normal launcher uses the current development ELF, separately
identified in `linux-loading-verification-2026-10-30.md`.

The compressed runtime archive includes provenance, exact recipe, license,
build/import/runtime receipts, checkpoint chain and separate artifact identities.
Archive readback/hash and owned raw-stage/private-store cleanup are separately
recorded in `out/validation/linux-current-20261029/linux-package-r5`.
Final runtime artifact:
`out/releases/OpenJuice-20261030-r5-linux-x86_64-nix.tar.xz`,
319,823,128 bytes, SHA-256
`f2a7b672f313a14169a84e0266ff086e8d160a11b0296116672f43b7cfc6507c`.
Every archived payload was fully read back and hashed. Both runtime/source
artifacts have adjacent checksum files. Only the owned raw export stage and
private import-store tree were removed after verification: 2,211,859,858 regular
file bytes, not a physical-space guarantee. Compressed artifacts, metadata,
source GC root and isolated installed profile remain. User data and real store
paths were not cleaned.
No original data, runtime mods, APK or user-save payload is included. Existing
RP5 remains APK19/car bundle r5; local APK20/car bundle r6 remain pending there.
Full integration still has the known strict original119-D8M failure; later
campaign/route/network/art/license/publication and physical-device work remains
open in the closure ledger.
