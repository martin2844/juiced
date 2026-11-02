# Native audio saturation study — 2026-10-31

The six-second diagnostic capture localizes saturation to addition of the 3D
sound bus and music before the final output clamp. The effects' captured PCM
bytes are intact: independent extraction from five original banks finds all
41 buffer payloads, representing 31 distinct PCM payloads. An independent
signed16 decoder exactly reproduces their native float samples. The final
clamp and actual successful SDL submission also match exactly.

This is a diagnosis of the recorded signal path, not approval of the original
Windows spatial levels or listening quality. No production gain, limiter,
normalization, source, asset, APK or installed-mod change was made. Development
and packaged Nix remain r14; the local signed Android candidate remains v26.
The RP5 was not accessed.

## Observation and independent reconstruction

Evidence: `out/validation/linux-current-20261029/audio-saturation`.
The observer is confined to an out-only copy of Audio.cpp and a separate
diagnostic executable. Removing its additive observation insertions reconstructs
the production source byte-exactly. Original DSP expressions, playback cursor
and state operations remain unchanged. Four other archive members and all
game/render/core libraries are reused; actual member hashes establish that only
Audio.cpp.o differs. The same 38 native mixer checks pass with the observer.

A private Xvfb/llvmpipe session follows normal title/profile/main/custom race
selection and records six seconds after Go. Virtual SDL input and diagnostic
player autopilot are used, with a dummy output sink. The observer records the
pre-clamp sum, post-clamp output, separate stream/3D/nonspatial contributions and
voice cursor/frequency/gain/pan state. It observes cumulative sums without
changing the native mix arithmetic. Source PCM and decoded buffers stay local.
There are no physical input, audible speaker, Android or FPS claims.

An independent Python parser reads the original loose audio-bank tables directly:
`3d`, `ui/race`, `cars/punto`, `cars/mx5`, `cars/golf` — 89 named entries. It
checks table/payload bounds, format, byte rate, block alignment and symbolic
sample names. Every observed raw payload and format matches an original entry.
It does not call the engine's bank parser, decoder or serializer. Independent
PCM decoding is bit-identical for all 41 buffers; none of those original signed16
samples is on an integer rail, and their maximum absolute decoded value is 0.988586.

Replaying raw voices from that independent decoding and logged state reconstructs
the 3D contribution with maximum absolute error 2.384e-7. Adding the observed music
stream reconstructs the full pre-clamp sum to the same tolerance. Group addition
differs by at most 5.96e-8, consistent with float addition order. The 288000 stereo
frames after the final clamp match the independently intercepted successful SDL
submission exactly, with zero frame shift. Seven copied-observation negatives
and five corrupted-bank/payload negatives are rejected; actual capture and
original files are unchanged.

## Measured levels

The capture contains 42 source buffers, 4126 voice observations and 282 callback
blocks. All logged buffer gains are between 0 and 0.899498. The nonspatial sample
bus is silent during this six-second racing interval.

| Signal | Maximum absolute sample | Samples above full scale before output clipping |
| --- | --- | --- |
| Music stream contribution |1.113020 |15 /576000 (0.002604%) |
|3D contribution |2.236073 |9579 /576000 (1.663021%) |
| Complete pre-clamp sum |2.481671 |20755 /576000 (3.603299%) |
| Post-clamp output |1.000000 |0 |

The one stream's full decoded float buffer has peak 1.479391. This study does not
independently decode that compressed music stream or establish its original
DirectShow output. Its possible overshoot must not be conflated with corruption
of the independently checked signed16 effects. It is a separate reference scope.

The actual output is exactly `clamp(sum, -1, 1)` in this sample. Consequently the
clipped samples are accounted for by real input contributions; no unexplained
PCM alteration, summation mismatch or SDL delivery corruption was found here.
This does not establish that the source gains or spatialization match Windows.

## Spatial reference and remaining uncertainty

The game's own library already performs distance attenuation. A source study of
[Wine's independent DirectSound implementation](https://github.com/wine-mirror/wine/blob/9e86063dcbfbccb6ed654143c435a6d974bb3a53/dlls/dsound/sound3d.c)
shows square-root speaker gains for 3D panning, whereas this port uses a linear
balance. The pinned source/hash is retained as a research reference. This
comparison identifies a question to measure; it is not a measurement of the
original Windows output and does not prove a particular replacement coefficient.
No Wine process or compatibility layer was introduced, and no reference code was
copied into the product. A level reduction solely to remove clipping would not
establish equivalence to the original behavior.

Next reference work must distinguish the original spatial gain/panning law,
music decoding/output behavior and audible mix quality. The analysis narrows the
problem; it does not mark those claims verified or change sound tuning.
An optional local six-second `race-audition.wav` converts the recorded post-clamp
floats to signed16 without normalization. It is not included in source updates.

## Runtime and preservation

The instrumented session has six entrants. Player 0 finishes fifth at 66.98s,
reaches results, returns and exits 0. Five finish events are observed. Car 4 is
still at waypoint 78, below 5 speed units in the last three logged observations,
when results begin. This is explicitly not a claim that all six finished. It
needs reproduction in an unmodified executable before attributing it to the
port or calling it a permanent stall; the observer may affect timing. That
follow-up is retained separately from audio correctness.

The owned game and private display both terminate. The controlled save archive,
all six personal Linux save/creative files, every installed Wave2 file and all
908 production input hashes match prior identities. The development ELF, APK26,
Nix r14 distribution and installed car bundle r8 remain unchanged. No public
upload/commit or device interaction. The broader verification goal remains active,
including the 119-D8M gate and remaining gameplay/art/device/network/reference
and final distribution/licensing work.

## Unmodified follow-up

`ai-recovery-followup-2026-10-31.md` records a separate normal-flow race using the
unchanged development executable without the audio observer. All six finish;
car 4 logs one mistake and finishes third at 64.63 s. The earlier low-speed state
is not reproduced, so it remains an observation rather than a confirmed defect
or an explanation to fix. No audio or AI tuning was introduced.
