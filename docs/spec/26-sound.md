# Sound

The game's sound library (`0x005B8000` .. `0x005C4400`) plays banks of samples through DirectSound 8 and streams of
Windows Media Audio through DirectShow. A manager of virtual voices gives the 3D sounds of a race the buffers of one
bank by their loudness every frame. OpenJuice follows it in `src/Audio` on the same DirectSound 8; EAX (through the
game folder's 32-bit `eax.dll`) only in the 32-bit build.

## The system

`0x005B8500`, the first time (`[0x0075F864]`, refcounted by every player and bank):

- The configuration's `audioflags` (`[0x0071E5D0]`, `JuicedConfig.exe`) with bit 1 (All Audio off): the library's flags
  (`[0x0071E718]`) become 1 and nothing is made; `[0x0075F860]` (the sound is on) stays 0.
- Otherwise the flags are the configuration's: 1 off, 2 hardware mixing, 4 software (set below), 8 no music, 0x10 no
  speech, 0x20 no EAX. 4 clears 0x20.
- Without 0x20, `0x005B84B0`: `LoadLibraryA("eax.dll")` (the current folder: the game's) and `EAXDirectSoundCreate8`;
  failing that, or with 0x20, `CoCreateInstance(CLSID_DirectSound8)` and `Initialize(NULL)`.
- `GetCaps` (`[0x0075EC18]`, 0x60 bytes): no 3D streaming buffers in the hardware (`dwMaxHw3DStreamingBuffers`,
  `[0x0075EC4C]`) takes 2 off and puts 4 on — always so on a computer of today.
- `SetCooperativeLevel(window, DSSCL_PRIORITY)`, `GetCaps` again, the primary buffer (`0x100D1`) set to 22050 Hz, 16 bits,
  stereo, then `SetCooperativeLevel(window, DSSCL_NORMAL)` (so the format asked for is not the device's) and the primary
  buffer played looping.
- With the device of `eax.dll`: the EAX probe and the reverb of FX slot 0 (below, "EAX").
- `[0x0075F860] = 1`.

## EAX

EAX 4.0 is reached through `IKsPropertySet` (property sets and ids of Creative's EAX 4.0 interface, the game's table
`0x006CC778` .. `0x006CC808`). It needs the device of `eax.dll`, a 32-bit library: the 64-bit OpenJuice never has it.

- The probe (`0x005B8C30`, only with the device of `eax.dll`): a temporary buffer in the hardware with a 3D buffer
  (`LOCHARDWARE | CTRL3D`, 0x40 bytes, 22050 Hz mono 16 bits, `[0x0075F844]`), its `IDirectSoundBuffer8`
  (`[0x0075F848]`), `IDirectSound3DBuffer` (`[0x0075F84C]`) and `IKsPropertySet` (`[0x0075F850]`; without it "No eax
  property set" to the debugger). Then `[0x0075F854] = 1`, the device and the primary buffer `AddRef`'d (never
  released for it), and `QuerySupport` (`0x005B9220`: get and set both) of the context (id 1), FX slots 0 .. 3
  (`0x10001`) and the source (1); each one not supported clears `[0x0075F854]`. The device's interface
  `{FA82931B-DE98-4A68-A7FE-D683E38A6502}`, when it answers `S_OK`, writes the device's flags into the system's `+0x0C`
  (its method `+0x10`): without 0x10 they are cleared, `[0x0075F854] = 0` and the release follows. The release also
  follows whenever `[0x0075F854]` ends 0.
- The start then (`0x005B88A6`): FX slot 0's `EAXFXSLOT_ALLPARAMETERS` = {`EAX_REVERB_EFFECT`, volume 0, lock 0, flags
  0} (`0x005B91A0`: a set through `[0x0075F850]`, false without it). Only when that fails: the slot's
  `EAXFXSLOT_LOADEFFECT` read (`0x005B91E0`, only while `[0x0075F854]`); when it is the reverb, its
  `EAXREVERB_ENVIRONMENT` 0x15 ("Reverb type could not be set" to the debugger when that fails) and the context's
  `EAXCONTEXT_PRIMARYFXSLOTID` FX slot 0.
- The release (`0x005B8F40`, and from the system's end `0x005B8320`): FX slot 0's `EAXFXSLOT_ALLPARAMETERS` =
  {`EAX_REVERB_EFFECT`, -10000, 0, 0}; when that fails or there is no property set, the slot's effect read (only while
  `[0x0075F854]`) and, when it is the reverb, its environment 0 (the same message on a failure) and the context's
  primary slot FX slot 0. Then the property set, the 3D buffer, the buffer 8 and the buffer released, `[0x0075F854]
  = 0`.
- A 3D set's buffers (`0x005C33C0`, the first at `0x005C3582`, each duplicate at `0x005C36A1`), the flags without 0x20:
  the buffer's `IKsPropertySet` into the voice's `+0x28` and its `EAXSOURCE_ACTIVEFXSLOTID` {FX slot 0, none} (the
  answer not looked at). Unlike the probe this does not need `eax.dll`: DirectSound gives most buffers a property set,
  and a voice with one does not ask `AcquireResources` when it is played (`0x005C3850`).
- Occlusion (the player's `+0x108`, `0x005C19A0`, from the virtual voices' `0x005C1360` and the voice given a buffer):
  the slot's set's `+0x88` (a 2D set's `0x005285D0` does nothing; a stream set has no such entry, never reached). The
  3D set's (`0x005C22D0`): without the system's flag 0x10 `[0x00725858]` (0.5) becomes `[0x0072585C]` (0.1) for good;
  the value is that × the occlusion asked; nothing more without the voice's property set; else the voice's `+0x1C`
  goes a step of 0.25 toward the value (below: −0.25, above: +0.25, not held at it) and its hundredths of a decibel
  (`0x005B8B50`) are set as the voice's `EAXSOURCE_ROOM` (flag 0x10) or else as FX slot 0's `EAXREVERB_ROOM` through
  the probe's property set when there is one.

`0x005B8B50` turns a volume in [0, 1] into hundredths of a decibel: `round(2000 × log10 v)` (x87), above 0 is 0, below
-10000 or for 0 and below -10000. The library's clock (`[0x0075F8A8]`, `0x005A2250`, `+0x0C` `0x005A2310`) is the
performance counter over its frequency in single precision, cut to whole seconds.

The game object makes the interface's manager with the sound at its start (`0x0040BAC0` → `0x005A5F90` →
`0x005B8500`); OpenJuice starts the system after the renderer.

## Banks

A bank is `audio\...\<name>\<name>.cfg`. The game's script reader (`0x004FB7C0`, `05-scripts.md`) reads it with one of
two tables of keywords (`0x00725060`: `[BeginBank]`, `bank_size`, `bank_name`, `instance_count`, `banktype`,
`declare_sample`, `declare_stream`, `[EndBank]`, `[BeginSS]`, `type`, `name`, `displayname`, `sample`, `instances`,
`[EndSS]`; `0x00725160`: the same with `[Begin]`, `CHANNELS` and `[End]`). A keyword of neither is passed over:
`trigger loop` (23 sets) does nothing; looping is the game's to ask.

- `CHANNELS n` (`0x005BEE20`): the player's slots become `(n & 0xFFFF) + 1`.
- `banktype t` (`0x005BE9B0` → `0x005BA370`): a sample bank (0 or 3: 0xBC bytes, vtable `0x006FEE88`) or a stream bank
  (1 or 2: 0xC0, `0x006FEEF8`); `bank_size` sets made (`0x005BDDF0`, 0x88 bytes each, vtable `0x006FEDB8`), the first
  current; the bank added to the player and the player's slots made again (`0x005BF6A0`).
- `declare_sample` / `declare_stream` (`0x005BAB40`): a name declared once (case ignored).
- `[BeginSS]` .. `[EndSS]`: a set; `name` (31 characters, upper case), `displayname`, `sample` (the declared name's
  index), `instances` (its buffers), `type` (0 2D, 1 and 2 streams, 3 3D).
- `[EndBank]` / `[End]` (`0x005BE000` / `0x005BE7A0`): the samples made.

The bank keeps two names: `+0x70` the `bank_name` (method `+0x14`), by which banks are found (`+0x20`, `0x005BA520`
over every bank made), and `+0x50` the `.cfg`'s name (method `+0x18`), of which `.dat` and `.dsb` are read. They
differ twice: `music.cfg` names its bank `Juiced`, `cruise.cfg` names its `3d` (so `0x005B9430` asked for `cruise`
never finds it loaded: every cruise race adds it again).

`<cfg name>.dat` holds a count and entries of 0xA4 bytes: `+0x00` the data's bytes (a stream: its length), `+0x04` 18,
`+0x08` a WAVEFORMATEX, `+0x88` the data's place in the file (the load adds the file's address at `+0x8C`), `+0x90` a
stream's start, `+0x94` the name. Without a `.dat` each set's sample is `<sample>.wav` (`0x005C1CE0`, `0x005C2D80`).

A set's buffers (`0x005C33C0`): the first with `LOCDEFER | CTRLVOLUME | CTRLFREQUENCY` and `CTRL3D` (type 3) or
`CTRLPAN`, the data copied in; the others `DuplicateSoundBuffer`. A 3D set's buffers get their 3D buffer and all its
parameters deferred (`0x005C1F40`: at the listener, cones of 360°, distances 1 .. 1e9). The set keeps the sample's
size, rate (`+0x04`, `+0x10`), milliseconds and bytes a frame (`0x005C41D0`).

A buffer's values (`0x005C42D0`, 0x2C bytes): frequency, pan, position, volume (hundredths of a decibel), play flags,
occlusion and the interfaces. Volume and pan are given only when they change. A 2D set's frequency (`0x005C3D00`) is
`round(asked × the sample's rate / 48000)` clamped to [the device's least, 48000]: 48000 plays the sample at its own
rate, and nothing goes above 48000 Hz. Played (`0x005C3850`): a 3D set in the hardware (`DSBPLAY_LOCHARDWARE`) with
flag 2 on, else in the software; a 2D set its pan first and in the software; without a property set the voice asks
`AcquireResources(DSBPLAY_LOCHARDWARE)` and a failure plays in the software; the position 0, the volume, a 2D set's
frequency, `Play` (a failure in the hardware tried in the software). A lost buffer is filled again.

## Players

A player (`0x005BEE80`, 0x44 bytes, vtable `0x006FF064`, its handle `0x006FEAE0` / `0x006FEBE0`) loads a bank (its
banks; `+0xE8` adds one loaded elsewhere) and has slots; slot 0 is never used. A handle is a slot's id: `id % slots` is
the slot, an id no longer the slot's answers 4, slot 0 answers 3, a slot without a set (stopped) 1.

- `+0x74` (`0x005C0130`): the first bank with a set of the name.
- `+0x04` (`0x005BF900`): a voice of the set and a slot. The voice by the strategies of its mask (`0x005BF2D0`, table
  `0x0071E738`): 1 a free one (`0x005C1B10`), 2 the quietest (`0x005C1B60`: the lowest volume, scanned from the last,
  unless all equal their mean), 4 the oldest start (`0x005C1C90`). A taken voice's slot is stopped and its place
  taken. Else the slot by its own mask (`0x005BF260`, table `0x0071E728`): 1 the first not held whose state is below 3,
  2 the set of the highest priority, 4 the quietest, 8 the oldest; one but the first is stopped. The slot gets a new
  id (`0x005BF8A0`: + the slots).
- `+0x18` start (`0x005BFA20`): the volume times the master, played, the voice's owner and start time kept, state 3.
- `+0x20` stop, `+0x24` pause, `+0x28` resume, `+0x30` state (4 looping, 3 playing, else 0; a fading slot playing 5),
  `+0x40` volume (now and as the target), `+0x48` frequency, `+0x5C` play flags, `+0x10` master volume.
- `+0xBC` (`0x005C0300`): the slots' volumes to their targets (up at once, down by 0.3 of the way, 0.0001 and below
  silent), a voice that ended frees its slot. Only `0x005BA110` (every player of the list `[0x0075F874]`) calls it, from
  the system's `+0x04` (`0x005B8AB0`) and the 3D world's `+0x08`. The game's main loop (`0x0040CCA0`) steps the
  system every frame after the front ends' frames (`0x0040CF0A`, with the frame's time), unless a job runs
  (`[G+0x40]`: the race's load and unload); the music steps it again as it starts and stops a song.
- `+0x108` occlusion (`0x005C19A0`, "EAX" above).
- `+0xC4` (`0x005BDD40` → `0x005C0580`), called by nothing: with the debug pages (`[0x0075F7A0]`) a red line per slot
  from 1 through their `+0x1C` with a font: `"%02d"` of `id % slots` without a set, else `"%02d : %1d : %12s %0.3f :
  %0.3f"` (the state, the set's name, the target volume, the volume × the master), from (25, 90), each line the font's
  height × 2 / 3 below, a second column from (380, 90) after every 24th.

## The interface's sounds

The interface's manager (`0x005A5F90` with a bank) keeps `+0xA8` the system, `+0xAC` a player of `audio\ui\<bank>`
(the front end and the game object: `ui`; the race front end: `race`), `+0xB0` the frame each set last played (one a
set of its last bank), `+0xB4` the effects' volume (0.5; `0x005A89F0` sets it from the settings' `+0x58` in the
front end's start, as a driver loads, in the options and around the loader), `+0xB8` the interface's frames and
`+0x104` the frame's time, at most 0.1 s (`0x005A6AB0`).

`0x005A8860` plays a set by name: the same set within 0.05 s (`[0x00725810]` over the frame's time, in frames; the first
set: within the same frame) plays nothing; else the slot and the voice by the mask (0xF, the options' Warning and
SPEECH 1), frequency 48000, volume times `+0xB4`, not looping, started. The windows' highlights play SELECT as they move
(`0x005A9F70`, `0x005B11A0`), a vertical list's items SWOOSH as each comes in (`0x005B0358`, the next item `+0x180`
from 1, reset with the menu `0x005AECF0`), the message box CLICK as it is answered (`0x005A9200`).

## The front end's music

The main function with the sound on (`0x00418300`): `0x0046BAC0` — unless the music is off (flag 8) one `rand()` drawn
for nothing and the bank `audio\ui\music1` added to the front end's player (always 1) — then `0x0046BDD0` and the
speech of the interface (`0x0040D330` with 1). After a race the same through `0x0046BA90`. `0x0046BDD0`: unless the
music is off, when its handle (`+0x0C`) is none, stopped, ended or fading, `music1` prepared (`0x005A8980`), at the
settings' music volume (`+0x10`, `+0x14`), 48000, looping, held, started.

The front end object's frame (`0x0046BFD0`) first moves its volume `+0x10` to the target `+0x14` (`0x0046BED0`): by
`(target - volume) / (1000 × the frame's time)`, held in `[0.001, 1]`; reaching 0.001 it becomes the target and is not
given that frame; otherwise given to the handle. `0x0046BD80(v, onlyTarget)` sets the target (and, unless asked not
to, the volume and the player's). The settings' volumes are given as a driver is loaded (`0x004679B0`), saved
(`0x00463F30`) and as "start" opens (`0x00467060`, with the game's sounds resumed as `0x0040D8E0(0)`): the music's,
the effects' (`0x005A89F0`) and, but for "start", the speech's (`0x00463DB0`: the speech player's master). CMSLoader
mutes the interface while it is up (`0x004CA910`, `0x004CA950`).

## The music

The game's music (`[G+0x48]`, `0x00408B60`, made once by `0x0040D200` with the system `[G+0x44]`): a player of
`audio\music` (one stream bank of 25 songs), `+0x10` the handle, `+0x14` the playlist's index (-1), `+0x18` the
volume, `+0x1C` its target, `+0x20` the song (-1), `+0x58` paused. The playlist (`0x00408D20`): the songs whose bit
the settings set, shuffled (`rand() % (k + 1)` for k = 1 ..).

- `0x00408E90` next: none → -1, else `index = (index + 1) % count`.
- `0x00408EC0(wait, force)`: with the music on: the next song when no handle and no song, or with force; not playing
  (`0x00409230`: states 2 .. 4) and a song: started at `+0x18` (`0x0040B330`: bank 0, 48000, not looping, masks
  0xF), the system stepped, with wait `Sleep(500)` (and one job of the main thread's queue), stepped again.
- `0x00408FA0(wait)`: stepped, `Sleep(300)` with wait, stopped, handle 0, `Sleep(300)`, stepped; unpaused.
- `0x004090D0(next)`, from the game's loop after the 3D world (`0x0040CECA`) and again at the end of the front end
  object's frame (`0x0046C837`): with the music on, a handle and not paused: silent (`< 0.001`) with a target of 0 and
  no preview (`[0x0075EF28]`) it stops; the volume goes by `(target - volume) / frame time / 500`, in `[0.001, 1]`,
  stopping when it falls to 0.001; ended with next: stopped, the next song played.

Callers: the garage (`0x00436C60` as carsel opens with a car neither damaged nor worn: the front end's music to 0
and, with the settings' music volume not 0, the next song played (wait, force) and faded in; `0x00436CF0` as it is
left: the game's music to 0, the front end's started again and faded back); the race's load done (`0x00463454`: the
next song (wait, force) at the settings' volume at once) and unload (`0x004634B0`: stopped with wait); the audio
options (`0x00480AD0`: in a race's pause the music plays only while MUSIC_BUT is highlighted).

The game's loop steps the sound system every frame (`0x0040CF0A`) after the front ends' frames, unless a job runs.

## Virtual voices

The race's world (`0x005B9260` → `0x005B9330`, `[0x0075F868]`): the system, a manager of virtual voices (`0x005BBB20`
with the channels: 350 in a race, 256 in the garage's scenes), the bank `audio\3d` in a player whose slots are the
buffers (33 with `CHANNELS 32`), the volume group 0 at 0.85; `+0x08` its volume going to `+0x0C` by `+0x10` a second
down and `+0x14` up. The race's load (`0x00461390`) sets the volume to the settings' effects volume, 20 and 50, the
groups 0.25, 1, 0.95, and adds `cruise` (a cruise, off line) and `wet` (a wet race; `dry` is never loaded).

An instance (256 bytes, vtable `0x006FEFF0`) comes from a pool (`[0x0075F894]`, the last free index first; none once
one is left) with an emitter (0xA4 bytes, vtable `0x006FF1B0`) and its state of the frame (0x4C bytes) of the same
index. The manager keeps them in lists (`+0x50` idle, `+0x54` starting, `+0x58` virtual, `+0x5C` on a buffer, `+0x60`
paused, `+0x64` stopping); a move puts an instance at the front of its new list.

Every frame (`0x005B96C0` from the main loop with the frame's time, `0x005BC8E0`):

1. `0x005BC3C0`: the virtual ones go on by the clock (milliseconds); one not looping ends (flag 1: freed, its owner's
   pointer cleared; else idle) — even while it would still be playing; a loop goes back to starting at its place.
2. `0x005BC650`: those on buffers that ended (not looping), or whose own length (flag 2) passed, end.
3. `0x005BCAF0`: the ones on buffers and the starting ones: the emitter placed (`0x005C14B0`: at the listener with flag
   4; at the node followed with its offset turned by the node's matrix; following another instance's place) and
   updated (`0x005C2B20`: once a frame the place against the listener's data, its change, the distance; the volume
   `gain × min / ((clamp(d, min, max) - min) × rolloff + min)` times the cone's); 0.01 and below: virtual, its buffer
   given back; else a candidate.
4. The candidates partly sorted (`0x005BD410`, MSVC's heap: the first slots - 1 by priority × volume, loudest first).
5. Each of those first: one with a buffer still its own keeps it (`0x005C1360`: volume, frequency, place); else a buffer
   (`0x005BCD10`: a free voice of its set, the quietest, the oldest; one taken from another instance moves that one to
   starting) and `0x005C11F0`: a loop's start drawn `rand() × 4 % its bytes` (given to the 3D player's `+0x50`, which
   does nothing), looping, the frequency `(its own + its random part) × the manager's pitch`, the volume (its group's),
   placed; a buffer not looping already is played. The others: virtual.
6. The listener committed, the stopping ones stopped.

A 3D voice's place (`0x005C20B0`): DirectSound's 3D buffer at the place against the listener, min distance 0
(`[0x0079E9C8]`, never set), deferred; with a doppler the frequency over `1 + clamp(place · change × doppler / (3430 ×
distance), ±0.3)`, cut, clamped to [least, 48000], given when it changed. The listener (`0x005C0870`, `0x005C0A10`):
distance and rolloff factors 0.001 — DirectSound only places the sounds; the library does their distance. At the
start (`0x005C0870`) the listener asks for the distance factor FLT_MIN and the rolloff FLT_MAX, deferred.

Windows' DirectSound refuses the min distance 0 and the rolloff FLT_MAX (`DSERR_INVALIDPARAM`; the rolloff's largest
is 10): the buffers keep their min distance 1 and the listener its rolloff until the first commit gives 0.001
(checked on Windows 11: `DirectSound.Refused`). Wine's (Proton) takes any value, and its attenuation of a 3D buffer is
`log10((d / min)²) × 1000` mB with `d = min + (clamp(d, min, max) - min) × rolloff`: with min 0 every 3D voice (the
cars, the world) is silent; the stock game under Proton has only its 2D sounds. OpenJuice does not ask for the min
distance 0 (as Windows, the buffers keep 1); the rolloff FLT_MAX is asked for as the stock does (nothing is worked out
with it: the places are deferred and their commit gives 0.001).

The listener's data (`world + 0x28`) comes from the players' frame (`0x004205A0`): the players' count; for each its
camera's world matrix and place, moved 100 cm back (views 3, 4, 6), 200 (5), and in views 5 and 6 neither panned nor
looking back 100 cm to the side of the look and 100 forward again.

The race camera's rain (`0x00401A4C`): in the wet `rain` (volume group 0, at the listener) looping, rolloff 0.0001; its
volume goes by 0.2 of the way a frame (`0x0040686C`) to `(1 - clamp(|camera y - ground| × 0.001, 0, 1)) × the rain`,
0 in a tunnel.

## Streams

A stream bank's sets are pieces of `<bank>.dsb` (one ASF/WMA file): type 2 (music) whole seconds of start and length,
type 1 (speech) floats. The stream (`0x005BB290`, 0x138 bytes, vtable `0x006FEA04`): a filter graph, the file added as a
source, its pin `Output` (or `Raw Audio 0`) rendered, media time. Played (`0x005BB660`): from `round(10000 × start) ×
1000` to that plus `round(10000.0 × length) × 1000` (100 ns units), run; its state is 1 once the position reaches the
stop. Paused streams are silenced and stopped; streams playing as the game loses the focus are paused and played
again with it (`0x005BB140`).

## The speech

The speech object (`[G+0x4C]`, `0x0040B150`, 0x2C bytes) is a player of `audio\speech\<name>` made by `0x0040D330`
(not with the configuration's no-speech flag 0x10): `ui<l>` for the front end (`0x0040BA40`, made at the start and as
the front end comes back from a race), `race<l>` for a race (`0x0040BA80`, made as the race's load starts), with `<l>`
the letter of `efgiscpr` by the language; its player's master is the settings' speech volume. The race's unload frees
it. Fields: `+0x0C` paused, `+0x10 .. +0x18` the set to say and its volume, `+0x1C` the delay, `+0x20` waiting,
`+0x24` the 3D world left alone, `+0x28` the handle saying.

- The interface's `+0x14` (`0x0040B640`): a line saying stopped, then set `(group − 1) × 33 + index` said at once;
  `+0x18` (`0x0040B750`): `6a + 264 + b`; `+0x1C` (`0x0040B6D0`): the set of a name.
- The race's `+0x14` (`0x0040B7B0`): set `(group − 1) × 16 + index` waiting; `+0x18` (`0x0040B7F0`): `35a + (x × y +
  16) × 8 + b` with two globals never written (`[0x0075EC90]`, `[0x0075EC8C]`). Both duck (`0x0040B420`): with the
  speech volume over 0 the 3D world goes (down 20, up 50) to half the effects' volume and the music to half its own;
  the line waits 0.3 s.
- Each frame (`0x0040B4D0`, from the loop before the music): a waiting line is said once its delay passes; with none
  speaking (`0x0040B5A0`: a handle not stopped nor ended, or one waiting) the 3D world (unless `+0x24`) goes back to the
  effects' volume and, in a race, the music to its own.

## The engine

Each race car's engine object (`0x00492D20` → `0x00492DC0`, the vehicle's `+0x498`) is made after the cars with the
sound on (`0x00498190`: each engine bank `audio\cars\<ENGINE_SOUND>[b]`, `b` with an exhaust part, added once to the
3D player; `0x00498410`). A master instance follows the car's node 90 up; the idle and the `_on` / `_off` loops of min,
low, mid and high (on and off filled each on its own), the splutters (an exhaust of level 2 and up), `trans_on` /
`trans_off`, a turbo's `turbowhine` and `dump<level>` / `dumpmed`, `gearup` / `geardown`, a nitro's `nitrostart` /
`nitroloop` all attach to it (rolloff scale 2, volume group 2). `.rev` (three tables of start rpm, end rpm and recorded
rpm) and `.vol` (three tables of four-knot curves, kept in the CarDef and their knots rebuilt from the ranges,
`0x00494360`) give each slot's weight by the rpm (table 0) and the on/off mix by the load (table 2).

The frame (`0x00494620`, from the car's sound frame `0x004875F0` after the road): the master volume (1.5 for cars the
camera does not follow, 1 for its car), the clutch's slip, the down-shift blip, the smoothed throttle, a splutter
(`rand()` every call, a second when it fires), the volumes, the transmission's whine by the driven wheels' spin, the
pitch with the shift's dip (`rand()` as a dip starts and repeats), the turbo's whine and dump (`rand()` as a dump
plays), the nitro. A gear up (`0x00495240`) or down (`0x004952A0`) is told after the frame. The camera's car takes its
sounds (`0x00401DB0`: priority 200, group 1, occlusion used, the road sound played, the horn's priority 200); every
other car's go to group 2 and from 350. In a reverb* helper (0.5) or with the camera in a tunnel* (0.75) the
camera's car's sounds are occluded (`0x00487320`). The replay records the engine's load and whether the horn sounds.

Each car's road sound (`tarmacdry`, `tarmac` in the wet, `sand` on a wheel over sand or grass) goes by its speed
(`0x00487490`); its horn `carhorn<n>` sounds while DRIVER_HORN is held (the player) or as the AI wishes
(`0x0049F360`). The wheels' `silence` instances take the suspension's `suspcrunch1` at the int 1's bits as the volume
(1.4e-45, not heard: a stock problem).

## The race's other sounds

- The effects (`SOUND_EFFECT`): a looping one draws its set's `rand()` as the emitter is made and sits stopped until a
  level plays it (`0x0050D1D0`: over SOUND_TRIGGERLEVEL the volume `(level / mul − trigger) / (1 − trigger) + minimum`
  times SOUND_VOLUME); a one-shot (`0x0050D2C0`) picks its entry by the level and its set by `rand()`.
- The impacts (`0x004916F0`): the explosion's sparks at `200 × speed + 30000` Hz, the flakes' one-shots (colsmall,
  colmedium, colbig by the level over 3, from 800, group 3), the scrape's loop (group 3). The squeal: the wheel of the
  largest skid's `4 × slide + smoke + skid`, from 500, at `|velocity| × 200 + pitch + 30000` Hz.
- The camera's car (`0x00495BE0` with `0x00492820`): its effects' scrape sparks and flakes sounds at priority 200 in
  group 1 (another car's at 1 in group 2); the occlusion of a reverb or a tunnel (`0x00487320`) also on the explosion's
  and the scrape's sparks, the flakes and the squeal of the surface, each with its occlusion on.
- The start's airhorn (`hk_airhorn`, 55000 Hz at the go, 44000 each second before it; at the route's first grid
  point, the route's `+0x4C`), the AI's grid skid
  (`gridskid`, its frequency drawn), the barrels' and bins' `trashcan` on a jolt (`rand()`), the cruise's crowd
  (`0x0041A560`: in a zone each delay a one-shot of the zone's trick or mood, `rand()` for the delay, the set and the
  frequency; four cooldowns of 1 s; the kinds' table kept from race to race).
- The race front end (`audio\ui\race`): CLICK as pause is asked, on the windows' choices and the replay HUD's
  commands, WARNING with the HUD's warnings (nitro, damage, the speed challenge, a showoff's move), WIPEOUT, the
  cruise's `warning`, `count`, `swoosh`, the respect's `swoosh` while it counts.

## Pause, replays and results

The race's pause (`0x004263E0`) pauses or resumes the music's song and the speech as it changes, and the 3D world's
voices. Start / restart replay (`0x00455BD0`, `0x00455E20`) stop the speech and pause the game's sounds (the race's
start also puts the 3D world at the effects' volume at once); a restart resets every emitter (`0x004261F0`,
`0x00426300`). restart_race's enter (`0x00455940`) does the same (the speech stopped, the game's sounds paused) and
they stay paused until its frame restarts the race (`0x00455A90`): then, with the race paused (as from hud_pause), the
music's song and the speech are given back; the 3D world's voices always are. Asked by the results with the race not
paused (the sprint's next heat), the music and the speech stay paused. hud_race_complete keeps the 3D player's master and over its last 4 s takes the world to a quarter of it
(down 400) and pauses its voices there; the sprint's results do the same at once; the challenge's and the cruise's
pause the speech and the world.

## The career race's chatter

The career's chatter (`[career + 0x68]`, built by `0x004B26E0` as the race resets: the player's own racer, its
crewmates, the racer bet on) runs at the end of each race frame once the count is over (`0x004B2930`), not in a
replay's race, with or without the sound: it draws `rand()` (a bet racer passing or passed by the player, finishing;
a crewmate's level changing to medium), gives a spun crewmate the order 1 (`0x004BF2F0`) and opens the crew's orders
(`0x004BEA60`). With a speech object, a line waiting or saying stops it for the frame; a line chosen is queued to the
race's speech (`+0x14` for the bet racer, `+0x18` for a crewmate by its number).

## The windows' 3D worlds

CMSCutScene (`0x004C6B80`) and car_rolling (`0x004DD490`) make a 3D world of their own (`0x005B9260`, `audio\3d`,
256 voices; `[G+0x50]`, the one before released) at the effects' volume; each frame they give its listener the view's
camera and step it (`0x004C7BF0`, `0x004DF260`), and the game's loop steps it again. car_rolling adds the car's engine
bank (once) and makes its engine sounds (`0x004DD300`: turned to the camera, rolloff scale 1, occlusion 1 each frame);
its leave unoccludes and frees them with the car and releases the world. The cut scene makes the same engine sounds
for each of its cars (`0x004DD300`, after `0x00486E90`); each frame its first car's are occluded 1 while that car's
stage is under 7, else 0.3 (asked even with no car in the stock); its leave (`0x004C7030`) unoccludes the first car's,
pauses the world's sounds (the manager's `+0x20`) and frees the cars. The cut scene's leave keeps its world until a
race's replaces it.

## The front end's windows

Beside the menus' CLICK, NO, SELECT and SWOOSH (`0x005A8860` with the front end's manager), the career's screens put
music1 at a quarter of the settings' music volume and back (CMSMeetingSummary, CMSRespectChanges, CMSRespect, the
phone, the cut scene; the character screen sets it to 0), the garage plays the game's music (`0x00436C60`), the dyno
takes it to a quarter (`0x004DD490`). The phone says its call's line and plays its rings (`[0x0075EF74]`); the respect
changes say their lines; CMSRespect narrates (`narrAC` .. `narrWA`, ducked; with no speech object the stock writes
through a null pointer). The song list (`0x004828A0` .. `0x00482D80`) previews a song with a 3.15/s crossfade from
music1 and back, its BACK restoring the ticks.
