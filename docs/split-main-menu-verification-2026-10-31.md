# Normal main-menu access to local split screen — 2026-10-31

The habitual Linux launcher now exposes local split screen through the normal
**Multiplay** entry. It opens the existing original **SPLITSCREEN / LAN MODE**
selector. Previously MainMenu routed directly to `link_search`, bypassing this
working screen; the prior completed split race had required an internal entry.
No replacement menu artwork or new race implementation was introduced.

The shared native Linux/macOS/Android route now selects `multi`; network startup
remains in its existing LAN action. Legacy Win32 routing is preserved by the
platform guard. The existing two-device requirement is retained: one device
disables local split, two allow it. LAN/search/back remain usable. macOS source
shares this change, but no macOS compilation/runtime claim is made here.

## Reproduction, correction and ordinary UI

Evidence: `out/validation/linux-current-20261029/split-main-menu`, including
**gallery.html**, original source snapshots, build logs and hash-bound receipts.
The new real-retail-UI routing regression fails before the source correction:
**13 passed/1failed**, specifically `menu: multi -> multi`. After correction:
MainMenu **15/0** and Multi **4/0**, including network-free opening, second-device
gating, local race mode/player count, retained LAN startup/search and back.
Native CTest **10/10** and Android host tests **4/4** pass. Complete original
inventory integration is **377/1**, still the same strict119D8M failure, with no
allowance, ignored failure or speculative decoder added.

Development ELF SHA256:
`2a03fb550556047faf828794c624ea25aaa885f5d60fdb2efbc438daa50ce084`.
Prior r10 development ELF is retained under the evidence directory.

A fresh owned process starts at **title**, loads the controlled synthetic
profile normally, selects Multiplay/local split, chooses the class/location/
route/one lap/time/weather, joins player2 and selects two Puntos. Both cars run
the real race, player2 pauses/resumes, and both finish: player2 **68.34s**, player1
**73.11s** in the race log. Results are full display size and contain both names.
The process returns to the selector, enters LAN search, returns to main and
exits0. Thirteen actual Radeon RX7800XT readbacks, all losslessly RGB-checked
against PNGs, document the flow; missing-phase controls reject incomplete logs.
The viewport is the actual1280x961 compositor client size. Virtual SDL pads
provide menu/pause inputs; diagnostic autopilot drives the race. This verifies
normal access and race/render lifecycle, not physical steering or performance.

The synthetic profile archive remains byte-identical. All6 personal Linux
save files, installed car bundle r8/Wave2revision7, physics/roster/save formats
remain unchanged. No RP5 interaction, installation, export or screenshot was
performed. RP5 remains APK19/packr5. This closes normal menu access and this
one actual two-player flow; wider split/render/input and network conditions
remain separate verification scope.

## Build candidates

Signed local Android ARM64 APK25:
`out/releases/OpenJuice-multiplay-arm64.apk` (10950766bytes), SHA256
`572ad6463c5e875c17ae78ef411e71ed6b0a5af1d6feea89e8389052753bf0d7`.
Version name `1.02.5-android.25-multiplay`. The certificate matches existing
installs; all7 ARM64 libraries, API26 dependencies, ELF and ZIP16KiB alignment
pass. Only `libmain.so` changes versus APK24; the six dependency libraries are
byte-identical. Native compilation and host tests do not certify physical APK
execution. APK24 remains an immutable historical candidate.

Clean Nix r11 output:
`/nix/store/ij72mv99yi56b1na8gi7kjp8r3gdzrk2-openjuice-1.02.5-linux-mods1`.
Actual ELF SHA256:
`f141d956b7cd5fca85f797199be54bb7cb89edf0f93e829891a0c197da2e473c`.
The build runs10/10 CTest and matches907 critical current source inputs. Exactly
three critical inputs differ from r10: MainMenu.cpp, MainMenuTests.cpp, and the
Android version metadata. All seven other prior shared-save-fix inputs match;
seven installed notices match. LGPL media dependencies are retained.

Exact r11 source companion:
`out/releases/OpenJuice-20261031-r11-nix-source.tar.xz` (5774472bytes), SHA256
`c60c69168640d030a41f48a639266d3a4428ecfc3c554be746fcf814cd9b54f0`.
All1342 files restore independently with the same source NAR and build
derivation. This proves the exact compiled input, not a second compiler run.
All307 closure paths import into a fresh owned private store with the same
dependency graph and content hashes; isolated installer/help pass. The actual
packaged wrapper/ELF runtime and final distribution receipt have their own
identities, rather than inheriting development-binary claims.

The actual packaged Nix wrapper was separately observed executing its expected
ELF through `/proc/<owned-pid>/exe`, rather than treating the wrapper's hash as
its executable identity. It completed the same normal-entry two-player flow,
player2 pause/resume, results and LAN/back. Race log finishes: player1 **64.23s**,
player2 **66.96s**. Eleven packaged readbacks are RGB-verified; selector, race,
results and LAN frames were inspected at full size. The synthetic archive
remains unchanged and the process exits0. Different race times are not a
performance comparison. Exact phases and missing-phase negative controls are
in `linux-package-r11/runtime-receipt.json`.

Final local runtime archive: `OpenJuice-20261031-r11-linux-x86_64-nix.tar.xz` (319702560bytes),
SHA256 `fa281710ee5a41386038b33344dcae34a315118f1d5412ecb103639de4789eb8`. All22 archive payloads were read back
and checked. Its standalone metadata binds the exact source/media companions,
current car bundle r8 and APK25; none of those or original game data/personal
saves is embedded. Public upload is not implied. Verified raw export/private
import copies are removed after packaging; immutable archives, metadata, sources
and receipts remain.
