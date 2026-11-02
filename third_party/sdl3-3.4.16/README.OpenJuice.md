# SDL 3.4.16 (the fixed build's controllers)

The fixed build reads the controllers XInput does not see (Bluetooth HID pads such as 8BitDo, DualShock / DualSense,
Switch Pro) through SDL, as JuicedConfigExt's plugin does with SDL3.dll, but linked in: no DLL beside the game.

- Source: https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16, `SDL3-3.4.16.tar.gz`
  (SHA-256 7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68), the release's `include` and `src`
  (without `src/test`) and its `LICENSE.txt`, `README.md`, `CREDITS.md`; the parts of `src` the build does not use
  left out (only the files its objects are made of and their headers are kept).
- Licence: zlib (`LICENSE.txt`).
- `OpenJuice.SDL3.vcxproj` builds the sources of the release's `VisualC/SDL/SDL.vcxproj` as a static library, for
  the game controller support alone: the joysticks (XInput, RawInput, HIDAPI, Windows.Gaming.Input, DirectInput,
  GameInput), the gamepads, the haptics, the sensors, the events, and the video subsystem's core they call (no video
  driver started: the game starts SDL with `SDL_INIT_GAMEPAD` only; the dummy driver alone is built); no audio,
  render drivers, GPU, camera, dialogs, storage or processes, the tray a dummy.
- `openjuice-config/SDL_build_config.h`: the release's Windows configuration with those subsystems off.

Altered source (zlib licence, 2): `src/dynapi/SDL_dynapi.h` turns the dynamic API off when `SDL_OPENJUICE_STATIC` is
defined (the project defines it), so that only the parts the game calls are linked in.
