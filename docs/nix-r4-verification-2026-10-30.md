# Nix r4 package verification — 2026-10-30

This is the packaged counterpart of the poker correctness checkpoint. It is a
local candidate, not a public release or full-game completion claim. No RP5
interaction was performed; the user is using that device.

## Build identity

The existing Nix build completed successfully; it was not restarted after an
observation timeout. It passes **nine CTest suites**. The output is:
`/nix/store/jfdghcpxv4wi170vfb6mq9950dhwgcqj-openjuice-1.02.5-linux-mods1`.

- Actual ELF: `bin/.juiced-wrapped`, SHA-256
  `71d6760f224a2f06f046e8c0593ae21d70893f86fad00a1d4328bd7f141944a4`.
- Wrapper SHA-256:
  `dcfc7f30644221b3e9bee3d2c94ced685b490e6343210c922765a1984476d0d8`.
- Derivation:
  `/nix/store/sz0xy4l5iyqk8x4gjhp3rmns5lhc6s9r-openjuice-1.02.5-linux-mods1.drv`.
- Actual compiled source:
  `/nix/store/mv672p9zkym48ng84a2zgi8y6419yzqf-source`.

The derivation's source binding independently matched **904 code,
build and tool files at the r4 build**, including both poker fixes and the Android version
metadata. Some documents were updated after the source snapshot; they are
separately checkpointed. The exact compiled recipe is included with the
package. This does not claim a byte-identical rebuild. The actual source is
retained through an owned indirect Nix GC root for later inspection.
The subsequent Linux upload-queue correction is a separate source checkpoint;
the immutable r4 archives predate it. See linux-loading-verification-2026-10-30.md.

## Actual package runtime

`run_actual_package.py` starts the wrapper, binds `/proc/<pid>/exe` to the
actual ELF, and uses only normal virtual-pad input/framebuffer capture. The game
boots through title, loads an owned synthetic profile, enters Career/Gamble,
confirms a five-player $1,000 cash hand, plays with normal opponent AI, settles,
performs the actual save-device autosave, returns to Career and quits normally.

The result banner shows **-$20**, and the independently inspected archive
agrees: **$40,000 -> $39,980**. Both original garage cars remain byte-identical,
as do members, crewmate statistics, events and eleven optional mod-progress
records. A separate r4 process loads and displays **$39,980** without changing
the archive. Its final Career capture contains one visible Gamble entry.

Final owned archive SHA-256:
`a24d0e2164f87a3d2655310a39df26244f3716cf108e00c97beff5c1fdf5cb06`.
Both game and private-display processes exit zero. Eight missing-phase controls
reject incomplete workflow evidence. No user profile is opened or wagered.
The private Xvfb/llvmpipe, dummy-audio and virtual-pad evidence does not certify
physical input, audible mix, Android rendering or FPS. The deterministic rule
edge cases remain separately covered by the source checkpoint's unit tests.

## Distribution verification

All **307 runtime closure paths** export successfully, import into an empty
owned local store, and have an identical dependency graph. Nix's full content
check passes, and the imported ELF hash matches the actual exercised ELF.
Running the package itself uses the normal Nix store; the private-store test
proves import/content integrity, not a second isolated runtime execution.

The installer succeeds with an exclusively owned test profile, and the installed
wrapper's `--help` succeeds. It does not replace the user's usual launcher,
settings, saves or game data. The usual launcher already uses the updated
development binary; the package's ELF identity is distinct and recorded above.

Archive construction, complete payload readback, final artifact hash and any
cleanup of raw owned staging are recorded separately under
`out/validation/linux-current-20261029/poker-package-r4`. The archive includes
neither original game data nor optional mods nor user saves. Its source
checkpoint list, runtime/import/build receipts, licenses and exact compiled
recipe accompany the runtime closure. No physical RP5 installation or public
publication is implied.

Final local runtime artifact:
`out/releases/OpenJuice-20261030-r4-linux-x86_64-nix.tar.xz`,
319,432,640 bytes, SHA-256
`0ffcbc39842e0312a06487e40e874571b115e153dd5755c194f682308accdc20`.
Every archived payload was read back and independently hashed.

The exact filtered source actually compiled is also retained as
`out/releases/OpenJuice-20261030-r4-nix-source.tar.xz`, 5,711,052 bytes,
SHA-256 `0ac7a20fb5f69e6cb9985a4e12434c529a5dc3c779b294a060cba9518177f1d1`.
All 1,312 source-file hashes match an independently extracted tree. Its NAR
identity matches the actual input, and instantiating the restored recipe
produces the same derivation recorded above. This verifies restoration of the
build definition; it does not claim a second independent binary rebuild.
Original game data and runtime mod packs remain separate from this snapshot.

Only the owned raw export stage and private import-store test tree are removed
after verification; compressed artifacts, metadata, source GC root and installed
test profile remain. The first cleanup encountered read-only imported Nix
directories. The retry changes owner permissions only on directories inside
that private tree, without following links or modifying real-store files.
The cleanup receipt distinguishes recovered stage sizes from the private-tree
remainder and does not equate file bytes to physical disk space reclaimed.

The complete current development integration remains **260 passed / 1 failed**,
solely the strict original 119-D8M inventory gate. Further campaign, route,
art/customization and physical-device coverage in the closure ledger remains
open. This package validation does not replace that broader completion audit.
