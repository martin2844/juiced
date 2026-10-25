# The race (`src/Game/Race`, `src/Core/Race`, `src/Core/Physics`, `src/Core/Tracks`)

How a race is loaded and run: the track, the cars, the racers, the physics, the camera, the replay. The windows over
it (the HUD, the pause, the results) are `race-frontend.md`. What the stock race does is `docs/spec`: 15 (tracks), 16
(physics), 17 (camera), 18 (racers), 19 (damage), 20 (effects), 21 (scene objects), 23 (showoff), 24 (replay).

## The pieces

- **`TrackView`** (`Game/Race/TrackView/TrackView.h`) is the race: the stock's race track object and what the game
  object keeps for it. Its members are the race's parts; its methods are in the `TrackView*.cpp` files beside it, by
  what they do:

  | file | what |
  |---|---|
  | `TrackViewLoad.cpp` | the load: the track's scene, the players, the physics world, the cars, the sounds' banks, the start |
  | `TrackViewFrame.cpp` | the frame: the physics, the racers, each player's frame, the laps' and the cruise's statistics |
  | `TrackViewCamera.cpp` | the cameras: the car each follows, its target, the fly-by |
  | `TrackViewScene.cpp` | the scene each frame: the lights on the cars, the projected textures, the wheels, the weather |
  | `TrackViewParticles.cpp` | the cars' particles: impacts, flames, skids, smoke |
  | `TrackViewSounds.cpp` | the cars', the crowd's, the road's sounds, the listener, the pauses |
  | `TrackViewReplay.cpp` | the replay: recorded, played, started again |
  | `TrackViewRestart.cpp` | the restart and the end, a car put back on the road |
  | `TrackViewWindows.cpp` | the race's side of its front end (`race-frontend.md`) |
  | `TrackViewControllers.cpp` | the players' controllers: their notes, the wheels' rumble |
  | `TrackViewNetwork.cpp` | the network race: the session's hooks, the watched racer, its overlay |
  | `TrackViewDraw.cpp` | the draw: each player's view, the racers' labels |

- **`RaceSlot`**: a car of the race: its body in the physics (`CarBody`), what the race knows of it (`RaceCar`), its
  model, damage, lights, particles and sounds.
- **`RacePlayer`**: a player of the race (one, or two in a split screen race; one without a car when watching): its
  car, controller, camera and its director, view, fly-by, the driver's control.
- **`Core/Race`**: the race's rules without a screen. `RaceState` and the competitors (`Racer.h`: the lines followed,
  the laps, sectors and places, the AI's steering and driving, the start's count; `RaceStart.h` an early port of
  the count alone, unused), the replay's record (`Replay.h`), the showoff's tricks and score (`Cruise.h`), the
  driver's control (`DriverControl.h`: the DRIVER1 actions into throttle, brake, steering, gears), the race of a
  career event and its bets (`RaceSession.h`), what a race is (`RaceKinds.h`: `RaceMode`, `RaceType`).
- **`Core/Physics`**: the physics world (`PhysicsWorld.h`), its rigid bodies (`RigidBody.h`), the car's body with its
  engine, drive and wheels (`CarBody.h`), the contacts (`Contact.h`), the touches with the route's collision
  (`TrackCollision.h`), the cars' physics files (`Dynamics.h`).
- **`Core/Tracks`**: a track's files (`Track.h`), a route's racing lines and sectors (`Route.h`), the scene objects of
  the `.dlh` files (`SceneObjects.h`), the ground maps (`SunMap.h`), the atmosphere (`Atmosphere.h`).
- **`Game/Race`**: the camera (`RaceCamera.h`), the fly-by before the start (`RaceIntro.h`), the grids by the race's
  mode (`RaceGrids.h`).

## The load

`runGame`'s Load phase (`app.md`) calls, after three frames of the loading window:

1. `TrackView::start(engine, setup)`: the track's scene (`scene::TrackScene`, `rendering.md`) of the route, time of
   day and weather; the players; the physics world against the route's collision; the effects' definitions; the
   sounds' banks. The load has its own `rand()` (`TrackView::loading`), as the stock's loading thread had.
2. The grid by the race's mode (`Race/RaceGrids.h`): the career's racers (their cars, the rival crew's scheme on
   copies), a custom or arcade race's, a replay's, the network's. Each car: `scene::CarModel::load` (the model),
   `TrackView::addCar` (its body, its race car, its particles, lights and sounds).
3. `startIntro` (the fly-by), each player's controller on DRIVER1, `beginRace`, `finishLoad`.

## The frame

`TrackView::drive(dt)` follows the stock's race frame (0x00425D40):

1. The frame's time smoothed (`PhysicsWorld::smoothTime`); a replay's restart asked done.
2. Unless paused, `raceFrame`: the drivers' levels and the crew's orders, the physics' steps (`stepFrame`: every body
   against the collision and the others, at the step time), the atmosphere, the replay's step (recording, or playing
   back the cars), each car's frame (its wheels, lights, projectors, particles, sounds), the racers' frame
   (`RaceState::update`: the count, the competitors, the places), each player's frame (its control, camera, finish),
   the cruise's.
3. The race front end's frame (`frontEndFrame`, between the race's watches of the controllers and the network).
4. The sound's listener on the camera.

`TrackView::draw` then draws each player's view of the world (with its post effects), the network's overlay and the
racers' labels, then the race front end.

Order matters as everywhere: the physics' order of bodies is the vehicles' manager's, the cars' frames run from the
second car to the last then the first where the stock does, and every `rand()` is drawn where the stock draws it.

## The physics

- `PhysicsWorld` steps its bodies at a fixed step time, the frame's time smoothed and split into steps; distances are
  metres (the world's centimetres / 100).
- A car is a `CarBody`: a `RigidBody` with the engine's and the wheels' spins in its state, the clutch, the gears,
  the brakes, ABS, traction control and the automatic gearbox, from the car's `dynamic_<car>.txt` with its parts
  fitted (`Core/Cars/CarPhysics.h`).
- The collision is the route's `.cld` (`Formats/Cld.h`, `TrackCollision.h`) and the bodies' `.ocd` meshes
  (`Formats/Ocd.h`); the contacts give the forces (`Contact.h`).
- The dents are the car's damage grid (`Core/Cars/CarDamage.h`): cells of a 16 x 16 grid around the body move the
  mesh's vertices; the zones add up the damage.

## The replay

Every quarter of a second (0.14 s in a cruise) each car's state is packed into the replay's ring (`Core/Race/Replay.h`).
Played back, the frames are read ahead, blended between and set on the cars; the cameras of the scene objects
(`SceneObjects`) and the director (`scene::CameraDirector`) cut between them. Saved replays are the stock's: the PC
game never gives its replays' save device an object (`Game/Race/ReplaySaves.h`, a stock problem kept).

## Testing

- `tests/Unit/Race`, `Physics`, `Tracks`: the race's state and start, the driver's control, the rigid body, the
  dynamics' files, the routes' lines, the atmosphere.
- `tests/Integration/GameData`: against the game's files, the tracks, the scene objects, the ground maps, the cars'
  physics (a car on flat ground, `tests/Framework/Fakes/FlatGround.h`), the replay.
- `tests/Integration/Race`: the race front end over a fake host.

The debug build's `--track <Track>/<Route> --car <car>` shows a route with that car on its grid, the first player's
and the AI's (`app.md`).
