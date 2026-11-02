# Drawing (`src/Render`, `src/Game/Scene`, `src/Game/Cars`)

How a frame is drawn: the render device, the stock renderer's programs and materials, the models, the 3D worlds of the
race and of the interface, the particles and the post effects. What the stock renderer does is `docs/spec/10-models.md`,
`11-effects.md`, `12-rendering.md`, `13-atmosphere.md`, `20-effects.md`, `21-scene-objects.md`.

OpenJuice draws with Direct3D 9, as the stock game does: the same states, the same shaders (the game's own compiled
effects), the same order.

## The device

- **`IRenderDevice`** (`Render/RenderDevice.h`) is the engine's only door to the graphics API. Everything drawn goes
  through it: textures and render targets, vertex and index buffers, shaders and declarations, the fixed function's
  states, the screen quads of the interface, occlusion queries, the frame (`beginFrame`, `endFrame`), the gamma ramp,
  the frame's capture.
- **`D3D9Device`** (`Render/D3D9/`) implements it on Direct3D 9; `createDevice` makes it from the platform's window
  and settings (`DeviceDesc`), on the settings' adapter. The fixed build's options (`Render/FrameOptions.h`: the
  texture filter and LOD bias over the sampler states, VSync, MSAA, the frame rate counter) and its window without a
  frame: a picture of another size than the window drawn into a surface of its own (the game's target 0 and screen),
  put into the back buffer at its place (`placePicture`: its shape kept with black bars, or stretched) as the frame
  ends; the screenshot is the picture alone. SMAA (JuicedConfigExt's `Shared/SmaaPass`) runs at the end of a camera
  view's scene; the world light's shaders (`Render/ShaderReplacements`, JuicedConfigExt's table) are made in
  the place of the game's as the effects create them (the effects keep the game's code and constant table); the heat
  haze (`Render/D3D9/D3D9HeatHaze`, `Game/Scene/HeatHaze`) after a camera view's particles, the cars' Heat_Haze
  emitters stepped and drawn there (`ParticleSystem::drawHeatHaze`).
- **`RenderStates.h`** names the fixed function states by their Direct3D 9 numbers (`RenderState`,
  `TextureStageState`, `SamplerState`, `TransformState`), so that the code sets what the stock code sets, by name.

## The stock renderer's parts

- **`Program`** (`Render/Program.h`): a compiled effect of `effects\material_*.fxo` (`Formats/Effect.h`) with its
  technique chosen by the detail and the shader classes, run on the device as the game's static effect library runs
  it (its passes, states, shaders and constants). `-localfx` compiles the developers' `.fx` sources instead
  (`Render/HlslCompiler.h`).
- **`Material`** (`Render/Materials.h`): an object per material of a model's part, made by its class from the `.pmd`
  values (`createMaterial`, the classes `ExeTables` names): it picks a program and binds its textures and constants.
  The classes are by family in `Render/Materials/`: the standard lit ones, the unlit ones, the cars' (paint, glass),
  the world's (road, buildings, sky, trees, sea, water), the sprites'.
- **`Model`** (`Render/Model.h`): meshes (`.d8m`) drawn by parts within their distance ranges with their materials;
  model nodes setting the object's constants. `Camera.h`: the scene's nodes and the camera.
- **`Renderer3d`** (`Render/Renderer3d.h`): the 3D state the shaders get as constants (the lights, the fog, the time),
  the material drawn last, the samplers. `Sky.h`: the atmosphere at draw time and the sky dome.

## The 3D worlds

- **`scene::Engine`** (`Game/Scene/Scene3d.h`) is the stock engine's managers: the programs made at start, the
  textures, meshes, materials and models read once from the game's folders (`texture`, `mesh`, `loadMaterial`,
  model loading), the reflection targets, the post effects and the lens flares' objects. `runGame` makes one for the
  game; every world shares it.
- **`TrackScene`** (`Game/Scene/TrackScene.h`) is a race's world: the route's geometry layers and their instances,
  materials, textures and baked light, the regions drawn from, the atmosphere, the ground maps, the scene objects and
  the particles. `draw` sets the camera's constants and the atmosphere, then runs the world's main pass.
- **The interface's worlds** (`Scene3d.h`): what a Begin3d element shows (a car turning, the garage's test track,
  the HUD's map), drawn in the element's rectangle (`ui.md`).
- **The cars**: `CarModel` (`Game/Cars/CarModel.h`) is a car's node tree with the fitted parts', the wheels' and the
  driver's meshes and its body's paint; `CarView` the interface's turning car with its mirror image, shadow and neon;
  `CarWheels` the race's wheels' looks.
- **What a world adds to its nodes**: the cars' drop shadow, neon and headlights' beam laid on the route
  (`Projector.h`), the cars' silhouette shadows (`Silhouette.h`), the lens flares (`LensFlares.h`), the sun's light on
  moving objects from the ground map (`SunLight.h`).
- **The fixed build's light cases** (`Render/LightCase.h`, `Game/Scene/LightSets.h`): a race's light is a dry day, a
  wet day, a dry night, a wet night or a dry day with cars' lights on in a tunnel (`lightCase`; the lights by
  `Game/Scene/TunnelLights`). Each case's set, in one table: the passes of WORLD LIGHT ULTRA it
  draws (the sun's cascades, the street lamps' shadows, the lights in the road, the light in the haze and its
  thickness) and the headlights' share of their strength. The pixel shaders of JuicedConfigExt's world light table
  the cases share (Standard Opaque 3.0, the day's road) are made for each case with its blocks within the 512 slots
  of the shaders 3.0 (`WorldLight.py`'s `CASES`: a dry day's the sun's, a tunnel's both, the others' the
  headlights'); the device binds the race's case's (`IRenderDevice::setLightCase` from `TrackView::draw`; a variant
  whose blocks are not all on is passed over). A case more is a `LightCase`, a row of the
  sets and a row of `CASES` (a `static_assert` keeps the two tables' bits alike).
- **The procedural clouds** (SKY HI and ULTRA, `Game/Scene/Clouds`, JuicedConfigExt's `Shared/CloudPass`): with HI
  a layer of clouds from a noise baked once, laid in the world's cloud shadows' own coordinates
  (`Renderer3d::cloudScale` in c46 and c47, read back by `cloudRows`); with ULTRA a volume of clouds in the world's
  own places (`CloudView::volume`: the baked noise their weather, a volume noise their shapes, 16 bits), the main
  view's clouds marched through it at half its size with the sun's light through it toward the sun and blended over
  the frames by the last frame's clip rows (`IRenderDevice::cloudView`, `CloudPass::DrawView`), the dome laying them
  over its colours, the reflections' sky marched in its own draws with fewer steps (`Clouds::reflectedView`), the map
  a window about the view; each view's map of their light takes the effects'
  cloudcast texture's place in every program (`Engine::setCloudCast`, `ProgramManager::replaceTexture`), the sky dome
  draws them over its colours (`SkyDome::clouds`, `IRenderDevice::cloudSky`), the sky's own clouds (`skyCloud`: the
  sky .dlh files' cloud quads) are left out (`TrackScene::setClouds` from `TrackView::draw` with the light case's
  set).
- **The nearest lights kept as they come and go** (`Game/Scene/NearestFade`): the street lamps the light in the haze,
  the lights in the road, the cars' shadows from the lamps and the cars' paint take are the nearest few of those
  found (`TrackScene::lampsWithin`, one more than kept); each is weighed by its lead over the nearest left out
  (`rankFade`: 0 as near as it) and by its distance's fade toward the reach (`distanceFade`), so the set changes
  without a light coming or going at once.
- **The frame's layers timed** (the debug builds' `--profile`, `Render/D3D9/D3D9Profiler`): the race's drawing marks
  each layer's start (`IRenderDevice::profileMark`: the headlights' and the street lamps' maps, the cars' cube, the
  occlusion depth, the sun's maps, the world, the cars, the post effects, ..; the shadow maps by the route's
  instances, the scene objects and the cars); the device times each on the GPU (timestamp queries, read some frames
  later) and the CPU and counts its draws and triangles. A layer whose CPU time is near its GPU time is held by its
  draws (some 0.6 microseconds each in Direct3D 9), not by its pixels.
- **The shadow maps' casters** (the fixed build's): the cars outside a map's camera (behind a headlight, off a
  cascade) left out (`TrackScene::carInMap`, a sphere of 4 m about a car's root; each car is some 40 draws), the
  route's instances under 2 pixels across from the view left out (`TrackScene::setCasterView`), and with one view the
  sun's two far cascades drawn by turns, the other kept with its frame of the frame before (`SunShadows::draw`). The
  cars' reflection cube of REFLECTIONS ULTRA draws its faces of one parity a frame, as the stock's below HI, and the
  cubes leave out the instances under 2 pixels across on a face (`TrackScene::collectCube`).
- **The cars' flames as lights** (WORLD LIGHT HI and ULTRA, `Game/Scene/FlameLights`): each shown exhaust flame
  (the nitro's, a backfire's) a warm flickering point light at its node, the eight nearest the view lighting its
  surfaces through JuicedConfigExt's `Shared/PointLightPass` (`IRenderDevice::pointLights`: each pixel of the ambient
  occlusion's depth taken to the world, its normal from its neighbours, the lights' falloff and facing, added over the
  view after the occlusion), at the light case's share (`LightSet::flames`: weak by day).
- **Particles** (`Game/Scene/Particles/`): the race's effects manager. `ParticleSystem` makes the emitters by the
  effects' definitions (`Core/Data/EffectDefs.h`); each kind is a class (`SmokeEmitter`, `SparksEmitter`,
  `TracksEmitter`, `CameraEmitter` for the rain and the ground rush, `LeavesEmitter`, `SmearEmitter`). They draw from
  the stock's lagged generator, not `rand()`.
- **Post effects** (`Game/Scene/PostEffects.h`): with an effect asked for, the view's scene goes into a render target
  and the view's end draws the motion blur and the glow (the fixed build no depth of field).

## A race's frame drawn

`TrackView::draw` (`race.md`), for each player's view: the post effects' target when asked for, then
`TrackScene::draw`, the world's main pass (0x00547690):

1. The world's cube when it is due, the cars' environment, the wet road's reflection (the scene upside down about the
   road under the camera).
2. The region the camera is in: its opaque instances, then the sky dome; the instances of the classes the stock draws
   after the cars wait for them.
3. The textures laid on the route (shadows, neon, headlights), the scene objects' and then the cars' node trees (each
   tree's late materials last: a car's glass over its driver), the lens flares.
4. The particles.

Then the view's post effects, and the race front end's windows over it all (`race-frontend.md`).

## Adding a material class

1. The class in its family's file of `Render/Materials/`, its parameters read from the `.pmd` bytes as the stock class
   reads them (`MaterialsInternal.h`).
2. Its name in `createMaterial`, by the material class `ExeTables` gives the `.pmd`.
3. A model of the game that uses it, drawn and compared with the stock game.
