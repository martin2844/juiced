# Native stereo pan correction — 2026-10-31

Fixed a production defect in the shared Linux/Android/macOS audio backend:
`IDirectSoundBuffer::SetPan` treated hundredths of a decibel as a linear balance.
Intermediate pan values were much too centered. Windows' separate DirectSound
implementation is unchanged. The habitual Linux launcher now uses the corrected
development ELF; a clean Nix candidate and signed APK26 are built locally.
The RP5 was not accessed or updated.

## Contract and failing-before evidence

[Microsoft's SetPan contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418148(v=vs.85))
defines channel attenuation in hundredths of a decibel, with one channel kept at
full volume and the other attenuated. The game's `VoiceSet::setPan` already
converts its linear requested balance into that logarithmic unit. The native
backend's old division by10000 interpreted that value in the wrong domain.

The backend now converts that attenuation into the mixer's linear channel
balance. Volume multiplication, center, endpoint silence and direct3D placement
are retained. No physics, rendering, asset, save-format or user-control change.

The persistent Linux regression in `tests/Linux/AudioMixTest.cpp` links the real
native audio library. Linker wrappers replace only hardware endpoint creation
and resume with a real SDL float-stereo queue. The actual native mix callback
writes to that queue and the test reads its samples. There is no production
test entry, mocked mixer, speaker requirement or test API added to the engine.

| Input / actual callback output | Before | Corrected |
| --- | --- | --- |
|0.5 input, pan+2000, left/right |0.400000/0.500000 |0.050000/0.500000 |
|0.5 input, pan-2000, left/right |0.500000/0.400000 |0.500000/0.050000 |
|0.5 input, pan+602, left/right |0.469900/0.500000 |0.250017/0.500000 |
|pan+2000 and volume-2000 |0.040000/0.050000 |0.005000/0.050000 |
|All mixer checks |33pass/5fail |38pass/0fail |

The same byte-identical test source runs before and after the production change.
Other checks cover centered/end/muted output, stop and one-shot termination,
half-rate interpolation, byte seeking, simultaneous-voice saturation, left/right/
forward3D positions and actual raw signed16 stereo decoding through SDL. An
independent verifier checks the measured channel values and rejects six bad
reports, including the original balance, reversed channels, lost volume factor,
missing case, nonfinite samples and all-silence output.

## Real executable output and complete races

Evidence: `out/validation/linux-current-20261029/native-audio` and the
`full-race-ui/native-audio-{private,nix}-r14` runtime directories. An observation
preload copies PCM only after successful real native SDL submissions. It does
not replace playback or modify samples, volumes, buffers or game state. Each
phase is capped at288000 stereo float frames, six seconds at48000Hz.

Both the ordinary development ELF and the new actual Nix wrapper/ELF follow
normal title/driver/profile load/main menu/custom-class/Punto selection/No to
auto-mod/race type/setup and a complete one-lap six-car race. They reach results,
return to the custom loader and exit0. The Nix process's actual executable was
observed through its owned `/proc` entry, separately from its wrapper hash.
Development player finishes sixth at73.56s; Nix player third at65.57s. These are
separate autopilot runs, not a speed/FPS or before/after performance comparison.

Each executable supplies six seconds of menu and six seconds of race PCM. All
four captures have the expected length, finite bounded samples, nonzero output
in both channels and distinct stereo content. Menu captures have no clipped
samples. Race captures reach the mixer's saturation limit on3.82% of development
and2.99% of Nix samples. This remains a recorded mixing/headroom/listening question;
the measurements do not establish that saturation is a new regression or justify
silently lowering volumes. Raw PCM stays local, with hashes/statistics retained
in the checkpoint; original music/sound files are not bundled as source updates.

The desktop was locked. The initial Wayland attempt could not receive controls,
entered attract mode and was closed normally; it is excluded from normal-flow
approval. Accepted runs use owned private Xvfb displays, llvmpipe, virtual SDL
pad input, diagnostic autopilot and dummy audio output. Both owned display
servers terminate. This proves generated PCM and these full game flows, not
speaker/headphone quality, physical input, Android execution or60FPS.
Transitional captures are retained as such; neither results capture is claimed
as a fully settled six-row results gallery. Race logs prove all six finishes.

Both controlled profile archives stay byte-identical. All six personal Linux
save/creative files and every installed Wave2 file match prior hashes. RP5
remains APK19/car packr5; Linux car bundle remains r8/Wave2revision7.

## Builds and immutable candidates

Native and clean Nix CTest pass18/18. Current ordinary unit checks388/0 and full
integration407/1: the strict original119D8M inventory failure remains visible.
Nix matches908 critical inputs; only Audio.cpp, Linux test registration, the new
audio test and Android version metadata differ from the preceding r13 input.
The306 dependency paths and seven installed license notices are unchanged.

| Artifact | Identity |
| --- | --- |
|Habitual development `out/linux/juiced` |`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f` |
|Actual Nix r14 ELF |`1f4e53e33dc93b8a17ca73a5a6599ac3a367b3483507eb88f48dce733cf37950` |
|Signed `out/releases/OpenJuice-audio-pan-arm64.apk` |`13de47b19015a44ec1f694c3e476e894a5a637eaa601f9495c9b7e3e122bc86f` |
|Exact `out/releases/OpenJuice-20261031-r14-nix-source.tar.xz` |`be3d28ccfbdbf84e77d6c63d85686fb8346bdcc8f38f985815480a2e413f52f0` |

Nix output: `/nix/store/1zhs7n4wcryzdchqq9ricwp9ig2wk6z8-openjuice-1.02.5-linux-mods1`.
All1345 exact source files are independently restored with matching NAR and
derivation. Source companion is5783976bytes. This is not a second compiler run.
The existing r9 LGPL media source companion remains applicable.

APK26 is10950766bytes, version `1.02.5-android.26-audio-pan`, with the existing
certificate. Only libmain changes versus APK25; the other six libraries are
byte-identical. Seven ARM64 libraries, API26 dependencies, ELF+ZIP16KiB alignment
and4/4 host tests pass. The actual Audio.cpp compile/object is recorded. No test
preload or audio-regression executable is packaged. APK25 is retained unchanged.
This is a cross-build/audit, not a physical Android listening or runtime claim.
macOS was not rebuilt in this Linux pass.

At this audio checkpoint, there was no public upload/push/commit and Nix r14
runtime export/import packaging was still pending. It was subsequently completed
with307-path import/content checks, isolated installer/help and23 archive payload
readbacks; see [Nix r14 distribution](nix-r14-distribution-verification-2026-10-31.md).
No public upload was made in that follow-up. The
incremental checkpoint preserves the failing-before evidence, four source/build
updates, final documentation and replayable verifiers. The broader game/assets,
mix listening/headroom, physical Android and distribution goals remain active.
