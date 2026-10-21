# macOS port status

## Provenance

- Upstream: https://gitlab.com/sidecr3w/openjuice
- Branch: `retail/1.01/dx9`
- Revision: `bd7e638b187e999c13f85a608d007108a228a05b`
- SDL 3: `829a65d769d935c4852f8159e964312c0957260a`
- MojoShader: `ad5dff84830c2863c841f4b1f4e3df78c705b383`

The original checkout is separate from this repository. No commercial game data, original executable or Windows DLL is needed at build time or committed here.

## Architecture

The existing C++ engine retains its game data, physics, race, career, frontend and save logic. CMake replaces the Windows platform/backend translation units with native implementations:

- SDL 3 window, keyboard events and Metal GPU device.
- A Metal implementation of `IRenderDevice`: textures, buffers, declarations, shader constants, render targets, pipeline states, frame presentation and capture.
- MojoShader converts the game's D3D shader bytecode to Metal at runtime. Local adapter fixes cover ps_1_1 texture-coordinate instructions and missing texture-coordinate stage inputs. Shader Model 1 sampler dimensions follow the bound texture, with cached Metal variants for cube-map reflections. Pixel alpha testing is applied outside the original bytecode, as D3D9 does.
- SDL audio mixer behind the existing sound-bank interface, plus FFmpeg WMA music decoding. Software mixing and no-EAX are selected explicitly.
- POSIX files, sockets and timers behind a narrow compatibility header layer; AppKit handles selecting the game folder. There is no Windows runtime emulation.

## Tested

The initial validation on the development Apple Silicon Mac loaded the supplied game assets, displayed the disclaimer and starting grid, and ran a 2,100-frame instant race with AI driving. The log recorded acceleration and progress through track segments, then clean process exit. Captures were visually inspected. This does not establish parity with the original renderer or validate the entire game.

The selected upstream unit suite reports 65 assertions passing. CPU identity tests are excluded because the upstream expectations are x86-specific. GameSpy voice tests are excluded because capture/voice has not been ported. The expanded data/frontend/race/save run reported **144 passes and 6 failures** (including the unit checks). Five failures are tests that enumerate loose effects, models, track definitions or ground maps: this installation keeps those in packed `.dat` archives, so their enumeration finds zero files. The sixth movie check requires eight audio tracks in `BIGGI_ACCEPT_BET1.bik`; this installation's Bink header contains five. The hub background movie decoded frame 30 successfully. These fixture mismatches are retained and reported, not marked as passing. Network and DirectSound-specific integration tests are not compiled.

All **137 extracted D3D shaders compiled successfully to Metal** using `juiced_shader_check`. A separate fresh CMake configure downloaded the pinned dependencies without development-cache overrides. The app executable was verified as ARM64.

A separate 1,600-frame race without autopilot verified scripted keyboard throttle and steering: speed increased after Up was pressed, and Right changed steering. Keyboard scripting also reached the title, profile manager and new-driver name entry. The native background Bink movie was visible in the profile manager. Save-device integration checks passed against temporary test folders; a complete career playthrough is still unverified.

Run the broader diagnostic suite with a separate save directory:

```sh
./out/juiced_tests "$PWD/out/integration-saves" --game /path/to/Juiced
```

This includes the loose-file and region-specific checks described above, which fail with the packed, five-audio-track installation used during this port.

To compile a directory of extracted, trusted shader bytecode against the actual Metal backend:

```sh
cmake --build out --target juiced_shader_check -j 8
./out/juiced_shader_check /path/to/shaders
```

Files must be `.bin`, one D3D shader each. No proprietary shader binaries are included.

The 0.2.1 reflection fix corrects legacy car/glass shaders that previously sampled cube maps as 2D textures. A standalone Metal regression checks six cube faces and alternates cube/2D bindings on the same shader, including a nonzero sampler slot. All 137 retail shaders still compile. A 1,800-frame race and a car-selection capture confirm corrected glass reflections; this is not a claim of complete lighting parity.

## Remaining work

- **Renderer fidelity:** visible differences in car materials/reflections and other scene effects. The fixed-function path is partial; `drawUp` does not yet execute programmable postprocessing. Multi-stage texture combiners, fixed-function lights/materials, fog, depth bias and some render states need further implementation/verification. Occlusion queries, gamma control and N-patches are unsupported. Developer `.fx` compilation is unavailable; precompiled game `.fxo` assets are supported.
- **Input:** keyboard with an in-game binding editor, persistent custom bindings and modern defaults. SDL gamepad mapping, wheel input and force feedback remain unimplemented.
- **Audio:** basic software playback; no EAX or full 3D listener processing. Music is decoded into memory by segment, so starting songs may pause loading. FFmpeg timestamp warnings can occur on the supplied music streams.
- **Movies:** intro playback is skipped; the upstream native Bink decoder renders the frontend background movie, but all intro/tutorial clips have not been validated.
- **Networking:** POSIX adapters compile, but multiplayer has not been tested. GameSpy voice capture is explicitly unavailable.
- **Game coverage:** profile creation, complete career progression, save/load round trips, split-screen and every track/weather combination still need end-to-end testing.
- **Distribution:** FFmpeg is a Homebrew runtime dependency. A self-contained signed/notarized app and Intel hardware validation are not provided.

This is a runnable native development port with substantial compatibility work remaining, not a completed replacement for the original game.
