# Sound and movies (`src/Audio`, `src/Game/Media`)

How the game's sounds, music, speech and movies are played. What the stock sound library does is
`docs/spec/26-sound.md`; the movies are `docs/spec/25-movies.md`.

## The sound library (`src/Audio`)

The stock game links its own sound library (0x005B8000 .. 0x005C4400); `src/Audio` is that library, under
`oj::audio`:

- **`SoundSystem`** (`SoundSystem.h`, `audio::system()`): the library's system over its output; switched off by the
  configuration's `audioflags` 1. `update` steps it each frame: the streams follow the focus, the slots go to their
  volumes, the ended ones are freed. `setSystem` puts another system in its place (the tests', of an output of
  theirs).
- **`Output`** (`Output.h`; `makeOutput` in `Output.cpp` picks the build's): what the library asks of the device, in
  the stock's order and with DirectSound's answers: the start (the primary buffer, eax.dll's EAX probe), the voices'
  buffers (their 3D and EAX properties), the primary buffer's listener, the streams of the stream banks, the movies'
  buffers. The library's logic (`Bank`, `Voices`, `Stream`, the movies' sound) holds no DirectSound of its own.
  **`DirectSoundOutput`** (`DirectSound/DirectSoundOutput.h`) is the stock's: DirectSound 8 made through the game
  folder's `eax.dll` when it is there (EAX 4.0, `Eax.h`), else by COM; the streams by DirectShow. The fixed build
  builds **`OpenAlOutput`** (`OpenAL/OpenAlOutput.h`) in its place: OpenAL Soft (`third_party/openal-soft-1.25.2`,
  static) answering as DirectSound does, the game's EAX as EFX (an EAX reverb in FX slot 0), the streams' WMA decoded
  by JuicedConfigExt's decoder (`third_party/wmadec`) into a streaming source (`docs/fixes.md`, Added).
- **`Bank`** (`Bank.h`): a bank of `audio\...\<name>\`: its `.cfg` names the samples or streams and the sound sets
  that play them, its `.dat` holds the samples; a stream bank plays pieces of one `.dsb`.
- **`Player`** (`Player.h`): the banks it loaded or was given, and slots ("channels") the game keeps by handle: a set
  is prepared into a slot and a voice, then played, its volume and pitch set by handle.
- **`Stream`** (`Stream.h`): the output's stream (DirectShow) playing a stream bank's `.dsb` (a Windows Media Audio
  file), a set a piece of it.
- **`Voices`** (`Voices.h`): the 3D sounds: a world with a manager of virtual voices, instances on scene nodes, the
  listener. Each frame the loudest instances get the buffers of the bank `audio\3d` (33 slots), the others go on by
  the clock. `setNodeMatrix` tells it where a node is (the game gives the scene's). A voice's place does not ask for
  the stock's min distance 0: Windows refuses it, Wine takes it and silences the 3D voices (`26-sound.md`).

## The game's sounds

- **The music** (`Game/Media/Music.h`, `jukebox()`): a player of the stream bank `audio\music`, the playlist of the
  songs the driver's settings allow (`GameData::shuffleMusic`), its frame each game frame, its stops and starts where
  the stock calls them (the race's load and end).
- **The speech** (`Speech`, `makeSpeech`): the interface's (`audio\speech\ui<letter>`) or the race's
  (`race<letter>`), one at a time, made and freed as the phases change.
- **`gameSounds()`**: what the game object keeps of its sounds for the parts that play them: the 3D world of a window
  or of the race, the speech, the driver's volumes (music, effects, speech).
- **The interface's sounds** (`Game/UI/UiSound.h`, `ui.md`) and the front end's music and sounds
  (`Game/Frontend/FrontendAudio.cpp`).
- **The race's**: the cars' engines (`Game/Cars/CarSound.h`: the engine's banks by its revs and load), the road, the
  crowd, the airhorn, the camera's rain, the pauses (`TrackViewSounds.cpp`, `race.md`). Where the race's rules in
  `Core` play a sound (as the stock's rules call the library, with `rand()` drawn in between), they call the hooks of
  `Core/Race/RaceSounds.h`, which the race fills in.

## The movies (`src/Game/Media`)

- **`movie::System`** (`Movies.h`): RAD's rad3d sample as the stock game compiles it in: a Bink movie's frames put into
  textures and drawn as screen quads, its sound through the language's track. The interface's Movie elements and the
  opening movies play through it.
- **Bink**: the 32-bit build loads the game's own `binkw32.dll` (`Binkw32.h`); a 64-bit process cannot, so the 64-bit
  build decodes Bink 1 itself (`BinkNative.h`) and answers the library's calls as the library does
  (`BinkLibrary.h`).

## Testing

`tests/Integration/GameData/MoviesTests.cpp` opens a game's movie as the stock code opens it (by a file handle, with
the language's sound track) and decodes a frame: through `binkw32.dll` in the 32-bit build, through OpenJuice's
decoder in the 64-bit one. The sound library needs a sound device; it is checked in the debug build's scripted runs
(`app.md`) and against the stock game.
