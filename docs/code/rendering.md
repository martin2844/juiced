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
  and settings (`DeviceDesc`).
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
- **Particles** (`Game/Scene/Particles/`): the race's effects manager. `ParticleSystem` makes the emitters by the
  effects' definitions (`Core/Data/EffectDefs.h`); each kind is a class (`SmokeEmitter`, `SparksEmitter`,
  `TracksEmitter`, `CameraEmitter` for the rain and the ground rush, `LeavesEmitter`, `SmearEmitter`). They draw from
  the stock's lagged generator, not `rand()`.
- **Post effects** (`Game/Scene/PostEffects.h`): with an effect asked for, the view's scene goes into a render target
  and the view's end draws the motion blur, the glow, the depth of field.

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
