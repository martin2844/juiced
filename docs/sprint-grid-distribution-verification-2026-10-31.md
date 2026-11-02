# Sprint grid fix — Nix r15 and Android27, 2026-10-31

The verified live-heat Sprint correction is now compiled into both new local
distribution candidates. The usual Linux launcher still uses development ELF
`e8450e7d…`, which already contains this fix. The RP5 was not accessed and neither
candidate was publicly uploaded or installed on Android in this pass.

| Local artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `out/releases/OpenJuice-20261031-r15-linux-x86_64-nix.tar.xz` |319,733,688 |`c326dd965c4ad322d71cf043e2086c6e1317ae5189dfd2df504cf5316f3a5d16` |
| `out/releases/OpenJuice-20261031-r15-nix-source.tar.xz` |5,819,168 |`0a6fbe4e3d5f69bc2f19980ebbee07d8a7a1b8343c51280e0cf2c807aee2e6a4` |
| `out/releases/OpenJuice-sprint-grid-arm64.apk` |10,950,770 |`00a1b9a650e9520e0c3c08ed9754a4c34684e1f196d3f56cf30b3f7284eb5f58` |

The exact r9 LGPL media-source companion and car bundle r8 are reused unchanged
and hash-bound in the runtime package's LOCAL-ARTIFACTS.json. Original game data,
mod assets, diagnostic observers and personal profiles are not bundled with the
engine. Nixr14/APK26 remain available as historical candidates.

## New Nix build and actual runtime

Clean `nix-build default.nix --cores 4 --max-jobs 1 -o out/linux-r15-result`
completed with18/18 CTest. All908 critical inputs match its actual compiled
source; two differ from Nixr14: the already verified TrackViewLoad.cpp fix and
the Android version metadata. Seven installed license/notice files match their
source files. The unchanged expected LGPL media paths are in the closure.

Actual output:

- Store: `/nix/store/sdh6iwif6nrzjkh35dl6bkvxwc9bxlyk-openjuice-1.02.5-linux-mods1`.
- Game ELF: `7cf6ca9dd21b34513ba8d94c2d5e2c8d6e30854cff984d599ca6a2ca83107422`.
- Wrapper: `04ee4aa701394bb4ea4f44a9ff107add90868e86f8412e8d6232f2e99ac6e11b`.
- Derivation: `/nix/store/9jan866c80xxf2fpg5kca64jfg0zqg87-openjuice-1.02.5-linux-mods1.drv`.
- Compiled input: `/nix/store/43j5w1srnzlf1rv51h4lj3b8as4ks7km-source`.

A fresh process entered through this wrapper, checked `/proc/PID/exe` and its
hash, loaded the owned natural seven-event profile and selected Novice3 Sprint
normally. Original437BHP SRT-4, four competitors, night/dry Engstrom route,
ordinary clock and virtual manual gamepad; no autopilot, forced route or edited
car/AI parameters. Three heats finished first in11.01/10.92/10.98seconds; final
18player points and normal results/leaderboard/menu return/exit0. All twelve
competitor finishes agree with independent point sums.

The passive read-only observer binds the actual Nix ELF separately. Its known
typed RTTI/vtable anchor is uniquely located through ELF sections; eight method
pointers are checked against executable sections. Independent development `nm`
confirms the resolver, and four corrupted-copy negative cases reject per ELF.
All360 bound native layout headers match the Nix compiled input. Runtime then
calibrates real neutral positions/headings, speed, gears and NOS against the
original SPL and ordinary pad responses. Observed starting grid indices3/0/1
confirm that the packaged runtime contains the working live-heat correction.
The observer reads via `pread`/O_RDONLY and never writes game memory.

There are3,557 telemetry samples,26 native BMP/PNG pairs,12 workflow/telemetry
negative controls and14 actually inspected grid/HUD/results/leaderboard/menu
panels. Both the complete private progression archive and the complete global
record table remain byte-identical to their seed. No new progress was invented
for the already-completed Sprint. Displayed times may truncate a centisecond
relative to rounded log times.

## Distribution and Android checks

All307 closure paths were exported and imported into a fresh private store,
then checked for exact dependency graph and store contents. The gameplay run
uses the normal Nix store; it is not claimed to execute from the private import.
The exact source companion independently restores all1,360 files with matching
hashes, NAR identity and derivation. This proves the actual input/build definition,
not a second independent binary rebuild.

The package includes the original attribution links, licenses, exact build/source
identities, bounded runtime proof and checksums for45 previous checkpoints and
the separate source/media/car/APK companions. Every compressed runtime payload
was read back and hashed; CONTENTS.sha256 was also exercised independently.
The unchanged installer creates an isolated test profile; its installed wrapper
and `--help` match the tested store output.

Large raw export/import temporaries were created only in the fresh owned
`/run/media/martin/Ediloco/OpenJuice-builds/20261031-r15` tree. The initial direct
installer exec failed with PermissionError because this data volume is mounted
noexec. The failure is preserved in `prepare-first-exec-failure.json`. The same
completed import was retained; interpreting the identical readable installer
with `bash` passed. No remount, permission change, re-export or duplicate gameplay
run was needed. The package documents `bash install.sh`. After verification,
only those owned raw export/import trees are removed; real store outputs, source
GC roots, isolated profile, archives, receipts and source restoration are kept.

APK27 is `1.02.5-android.27-sprint-grid`, ARM64/minAPI26. Release build succeeds,
signature certificate remains
`a7ebaad7de021b73f8498f9b60c81c601e321560e362de223cf2d4fb33741e35`.
All seven ELF libraries have resolved packaged/API26 dependencies and compatible
16KiB LOAD alignment; SDK zipalign16KiB check passes. Four Android host tests
pass. The actual TrackViewLoad.cpp native compile command/object is retained.
Compared with APK26, only `libmain.so` and AndroidManifest.xml payloads differ;
Java dex, six auxiliary libraries and the entire ZIP entry set are unchanged.
These are cross-build/package checks, not physical Android runtime approval.

## Preservation and remaining scope

The development ELF/default launcher, installed Wave2 r8 and all six personal
save/creative files retain their hashes. No game data, car assets, device state,
user profile or public repository was changed. Only Android version metadata was
edited in this pass; the Sprint C++ correction was verified in the prior checkpoint.

Physical APK27/car-r8 installation and save/visual/input parity remain pending
while the user uses the RP5. Braking/steering/downshifts/handbrake, broader natural
career/team/network/fidelity checks, strict119-D8M handling and final source/
license/publication review remain separate open requirements. No60FPS or
complete-game claim. The overall verification goal remains active.
