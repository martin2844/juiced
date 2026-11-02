# Linux preview loading correction — 2026-10-30

Following the user's request to leave RP5 alone and continue on Linux, the
packaged Arcade run showed black preview panels before the entire asynchronous
car/mirror/shadow job finished. A later settled capture confirms that the models
do appear. The defect is excessive upload latency, not permanently missing cars.

The Linux renderer serviced one snapshot of its GL-owner queue per frame.
Synchronous loaders enqueue the next request after the preceding one completes,
so each request could wait a whole additional frame. Android already has a
bounded follow-up service loop; this change applies that same approach to Linux.

`GlesDevice::letLoadingRun` now services follow-up requests within a four-ms
slice. A condition variable wakes it when the producer enqueues work. Empty
queues return immediately, the queue mutex is released around callbacks, and
all GL work remains on the context owner. The deadline bounds scheduling and
waiting; it cannot preempt an individual GL operation. Android production code,
car assets, physics, roster and save formats are unchanged.

## Evidence

The existing sequential-upload regression now builds as
`juiced_loading_queue_check` against either platform renderer. It requires a
real GL context and is a manual check, matching the existing GL regression;
it is not silently added to the sandboxed/default CTest set.

On Radeon RX 7800 XT, 161 sequential upload/update/destruction requests:

| Version | Service frames | Elapsed |
| --- | ---: | ---: |
| Original Linux queue | 162 | 2,676 ms; regression fails |
| Corrected Linux queue | 2 | 36 ms; regression passes |

The regression also checks that 100 empty-service calls do not accumulate the
active-loading budget. Full native build and nine CTest groups pass. The real
Radeon GL check passes its four groups, covering cube/2D switching, loading-thread
texture creation, neon/additive and constant blending, transformed buffered
vertices and depth preservation. Full integration remains **260 passed / one
failed**, the unchanged strict original 119-D8M inventory failure.

Normal title/profile/Arcade navigation, ordinary virtual-pad input and framebuffer
readback compare the same owned profile with both the old packaged ELF and the
retained old development ELF. The first Prelude preview, including car, mirror
and shadow log completion, takes **6.707 s** in Nix r4 and **5.981 s** in the old
development binary, versus **0.452 s** in the corrected development binary.
The subsequent Corsa selection takes 0.546 / 0.507 / 0.246 s respectively.
Captures wait for both models and the shadow, then another 1.5 s for attachment
and fade. Settled Prelude/Corsa captures were manually reviewed with visible
cars/reflections; BMP/PNG pixels match. The earlier blank captures were premature
and are excluded from art approval.

These are single-run local observations under private Xvfb/llvmpipe, including
input/map overhead, not a formal benchmark or an Android/FPS result. A separate
current-binary Radeon sample loads the installed r6 405 driving model, reaches
Go and applied AI movement, and exits normally after 600 frames. Its capture was
manually reviewed; this is not a second full race or physical handling test.

## Identity and preservation

Current development ELF SHA-256:
`e2c88a434d4b9740004ba1ceea6f3e7fdd8d95687d86d126b84e136f5d53e449`.
The usual `~/.local/bin/openjuice` launcher already executes this path. The old
development ELF is retained under the validation directory with SHA-256
`4e21db0ba62aab56383fed55488dc9149fd67364163d97c462f0ce4dc479789b`.
Historical evidence remains tied to its original binary; its expected hashes
must not be rewritten to the new executable.

Exactly four code/build/test inputs differ from the Nix r4 source receipt:
Linux GlesDevice header/implementation, Linux CMake and the shared loading test's
backend include. The other 900 checked inputs match. All six actual local
profile files and the installed Wave 2 package retain their recorded hashes.
The normal launcher is not replaced. No RP5, Android APK, publication or user-save
operation was performed. The immutable Nix r4 runtime/source archives predate
this correction; the source update checkpoint supplies the four changes.

Evidence: `out/validation/linux-current-20261029/linux-loading`,
`full-race-ui/arcade-preview-loading-{before,before-dev,after}`.
`linux-loading/verify.py` binds source, binary, logs, images and preservation.
One initial CTest invocation lacked `ctest` in PATH and is excluded; the explicit
Nix-store executable invocation passes. Wider closure requirements remain open.

Later follow-up: the corrected Nix r5 package completes the four-lap Novice final
and separately reloads its earned series completion. Its runtime/source archives
are now verified. This extends the earlier finite driving sample; it does not
rewrite its historical scope. See `novice-series-verification-2026-10-30.md` and
`nix-r5-verification-2026-10-30.md`.
