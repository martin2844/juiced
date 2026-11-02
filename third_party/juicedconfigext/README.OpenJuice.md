# JuicedConfigExt's core (the fixed build's controls)

The fixed build's controllers are those of JuicedConfig Extended (https://gitlab.com/sidecr3w/juicedconfig-extended,
the git submodule `external/JuicedConfigExt`): its core, `external/JuicedConfigExt/src/Shared`, the same its plugin
(JuicedConfigExt.asi, for the stock game) and its settings program build. `OpenJuice.JuicedConfigExt.vcxproj` builds
it as a static library for both platforms: the controller layout of all profiles and its file
(`ControllerLayout.h`, `LayoutFile`: `%APPDATA%\THQ\Juiced\JuicedConfigExt\controller.ini`), the controls file of
the game with the layout's controls (`ControlsFile`), the controls' values (`PadActions`), the controller's reading
(`PadReader`: XInput, else SDL3, linked in from `third_party/sdl3-3.4.16`), the steering wheels' marks, map and
reading (`WheelDetect`, `WheelMap`, `WheelReader`: DirectInput's raw states in a thread) and the force feedback's
scale (`ForceFeedbackScale`). A change of the core is made in JuicedConfigExt and the submodule moved on to it.
Its licence is OpenJuice's (PolyForm Noncommercial 1.0.0, `external/JuicedConfigExt/LICENSE.md`).
