# Unmodified Linux race follow-up — 2026-10-31

The low-speed car 4 observation from the audio diagnostic was not reproduced in
this follow-up. The actual habitual-launcher development ELF is used unchanged,
SHA-256 `556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
The observer and audio intercept are absent. Only virtual SDL input and framebuffer
readback preloads remain. This is a private Xvfb/llvmpipe run, with diagnostic
player autopilot, dummy audio, seed 42 and an owned copy of the same profile.
It does not establish physical input, Android behavior, audible quality or FPS.

Normal title/profile/main/custom selection reaches a one-lap six-car race.
The run records these actual finishes:

| Car | Place | Race time (s) |
| --- | --- | --- |
| 5 | 1 | 64.01 |
| 2 | 2 | 64.35 |
| 4 | 3 | 64.63 |
| 0 (player) | 4 | 64.82 |
| 1 | 5 | 65.14 |
| 3 | 6 | 66.03 |

Car 4 logs one deliberate AI mistake and continues beyond waypoint 78, completes
the lap and finishes third. All six finish events have already arrived when the
results window is detected. Therefore the helper's up-to-45-second post-results
observation ends immediately; this is not a 45-second stall-recovery experiment.
The original diagnostic trajectory is not deterministic across these runs.
This result does not dismiss that earlier observation, prove recovery from the
same low-speed state or justify a physics/AI change. No production code changed.

Source review locates mistake handling and stuck/reverse/respawn timers in
`src/Core/Race/Racer.cpp`; results support an unfinished row while other racers
continue. This review is a guide for a future targeted reproduction, not new
original-binary equivalence evidence.

Three native BMP/PNG pairs match pixel for pixel. The results capture was visually
inspected and is still the overlay's entry transition; it is not an approved
settled six-row results-table screenshot. Finish claims come from actual runtime
events, not that image. The game returns to custom loader and exits 0; the owned
private display exits 0. No additional process is left running.

All 908 critical production inputs, every installed Wave2 file and all six
personal Linux save/creative files preserve their prior hashes. The controlled
ModSaves copy is unchanged too. APK26, packaged Nix r14 and car bundle r8 remain
current. No RP5 access, installation, public upload or commit was performed.

Evidence: `out/validation/linux-current-20261029/ai-recovery-r14` and
`full-race-ui/ai-recovery-unmodified-r14`. `verify.py` checks binary/probe identity,
ordered navigation, six entrants/finish events, original seed save files,
BMP/PNG equality, preservation and terminal process receipts. The broader goal
remains active, including the strict 119-D8M gate and physical Android/art/reference
and distribution/licensing requirements.
