# Original mesh-factory caller follow-up — 2026-10-31

Four new native dossiers extend the previous18 to22, reusing the exact original
PE and matching REA snapshot. This narrows known caller gaps; it does not resolve
the strict119-D8M inventory failure or prove whole-program unreachability.
No production parser, asset or diagnostic changed.

Target: `/home/martin/Projects/juiced/game-data/Juiced.exe`, SHA-256
`6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b`.
REA/Ghidra12.1.2 performed static analysis; the original game was not executed.
Evidence bundle and analysis snapshot were exported, and the REA session closed.

| Procedure | Observed boundary | REA Evidence |
| --- | --- | --- |
| 0x48a820 | Direct calls to file factory0x554610; literals `%s_LOD`, `%s_BS` and `RX7_WHEEL_BLUR` | `ev_20289f681bf89d30502d9364cfd0eb86f1a8863f5ee4d2b060ec5070bd846030` |
| 0x48b5b0 | Direct call to the same factory, with recovered implicit-register inputs; caller0x48b760 | `ev_cabc23c5e541b56d793c9a0e9c7a989ec0a5650347f8cb8892b8e6afe627fd0b` |
| 0x48fb00 | Same factory called with the literal `exhaust_flame`; caller0x496eee | `ev_87639bef7f51e03a3e0c7b63680b4f366fef1d19801d1602fcecdd6ace5eb615` |
| 0x54ef00 | Dispatch case1 reaches0x54e500, the previously inspected release-layout stream chain; caller0x543115 | `ev_088d53aada2d9bea4c4cc552502f425c670aeff73d6cf54a53811211eed354df` |

The fixed flame/blur names are not among the119 rejected payloads. This is a
bounded filename comparison, not evidence that every dynamic name resolves.
Current-source wheel filling in `src/Game/Cars/CarModel.cpp` includes those
blur/material suffixes; `src/Game/Race/TrackView/TrackViewParticles.cpp` loads
`exhaust_flame` under the car folders. Those source anchors provide context,
not proof of original runtime behavior.

`out/validation/linux-current-20261029/legacy-factory-callers-r14/record_factory.py`
checks each new call independently in raw GNU objdump output across the
provider's complete reported body ranges. It independently maps PE sections
to file offsets and checks every new referenced ASCII literal, including its
terminator and source hash, against original bytes. All119 original rejected
payloads are independently reread/hash-matched and all908 critical native
source/build inputs remain unchanged.

The new bundle retains24 evidence records:22 function dossiers plus the two
prior byte observations. `receipt.json` binds the bundle, snapshot, raw assembly,
function ranges, string offsets and original/source identities. No original
executable bytes or raw game assets are added to the checkpoint.

Remaining limits are material:0x48b5b0's recovered inputs do not establish a
complete ABI or filename producer. Other0x54ef00 dispatch branches have not
been proven equivalent or unreachable. Noncontiguous function bodies and
Ghidra terminal-flow/calling-convention warnings remain; pseudocode is not
original source. The REA ledger's zero unknown count does not discharge these
explicit uncertainties. Static references are not execution observations.

No guessed legacy format, empty-file acceptance or inventory exclusion was
introduced. Original whole-program/runtime reachability remains open. Continue
from these retained records and the earlier `legacy-mesh-audit-2026-10-29.md`,
without repeating already completed analyses or treating this as a gate pass.
