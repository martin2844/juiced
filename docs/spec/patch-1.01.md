# Patch 1.01

What the official patch 1.01 (July 2005) changes, and what this branch (`retail/1.01/dx9`) does about it. The branch is
`retail/1.00/dx9` with the patch's changes on top; addresses of the form `0x0041FF7E` are those of the official 1.01
executable, the others those of the build `retail/1.00/dx9` follows.

## The patch's notes

    Collision issue when playing online
    Various desktop crashes
    Sound problems
    Increased deadzone for input devices from 1% to 20%
    Fixed conflict with Windows Media program
    Play on computers that have more than 2 gigabytes (GB) of memory (RAM)

## How it was compared

The official 1.01 executable was compared with the retail 1.00 builds function by function.

The build `retail/1.00/dx9` follows was made for another region from later sources than the English 1.00: it already
has most of 1.01's changes, among them:

- the engine sounds' changes (the turbo's spooling among them: `0x00494D00`'s 0.14 is 0.64, `0x00487FE0`,
  `0x00492DC0`);
- no memory check (1.00's `GlobalMemoryStatus` in the window's set-up is gone: the "more than 2 GB" note);
- the eight languages (`controls_dx9_<c|p|r>.txt`, the letters `efgiscpr`) and the language picked from the registry
  as 1.01 does (`0x0059E300`);
- the race front end's and its windows' changes (a field more in the race front end, the race's windows').

## What this branch changes

### The input's deadzone (`0x005920F0`)

An analogue axis of `controls_dx9` whose `Deadzone` is 1% (from 0.009 to 0.011) takes 20% (0.2) in its place before
the axis is set up (its scale `1 / (saturation - deadzone)` with it); any other deadzone of the file is kept as it is.
`src/Core/Input/Input.cpp`.

### The keyboard's steering (`0x0041FF7E`)

Without a steering wheel the input is shaped by |s| to the power `1.1 - 1.1 x sensitivity + 0.07` (a float,
`[0x007272E4]`) where 1.00 added 0.05 (a double): the curve is a little steeper. Not in the patch's notes.
`src/Core/Race/DriverControl.cpp`.

### Collisions online (`0x004996F5`)

In the cars' frame (after each car's lights, `0x004890B0`), in a network game the player's car keeps its race
distance in a global (`[0x00760F40]`, new in 1.01), and another machine's car whose race distance is within 30000 of
it (strictly) collides again (its body's `+0x78`, cleared by its net state, set back to 1). Then with the matchmaking a
session hosted without collisions (the online kind's session: the arcade one's `+0x34`, the career one's `+0x44`)
takes every car's collisions off. `src/Game/Race/TrackView/TrackViewFrame.cpp`.

### The device's creation (`0x00514EBB`)

`IDirect3D9::CreateDevice` is asked up to 250 times: each turn tries the device, then again with the depth format D16
(kept from then on); when both fail the game sleeps 40 ms (`Sleep`) and tries again. 1.00 gave up after the two tries.
Most likely the "Windows Media" note: the device taken while another program held the display.
`src/Render/D3D9/D3D9Device.cpp`.

### Seen and left alone

- The window properties `Caption`, `AddItem` and `AddTexturedItem` (`0x005AE560`, `0x005AF2A0`, `0x005AF3E0`) check
  a text's number against the window's texts and take an empty text outside them; the port's texts give an empty
  text there already.
- The cars' frame passes its manager in a register (`0x00427A47`): the compiler's doing, no change of what it does.
