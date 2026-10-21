# Movies

The game plays Bink movies (`movies\*.bik`, version `BIKi`) through RAD's `binkw32.dll` and the rad3d sample of the
Bink SDK compiled into the executable (`0x0055AB80` .. `0x0055B8F0`): the frames go into textures and are drawn as
screen quads. The interface's Movie elements (`04-interface.md`, "Movie") play one movie at a time.

OpenJuice calls the game's own `binkw32.dll` in its 32-bit build. The 64-bit build cannot load a 32-bit library: it
decodes the Bink 1 files itself (`Game/Media/BinkNative.*`: the planes' bundles of 8x8 blocks, the YUV to the surface type
asked for, the Bink audio tracks (DCT or RDFT) through the game's DirectSound on a thread of their own) and answers the
same calls. Its pictures are byte for byte `binkw32.dll`'s (all 472 movies, every surface type): the residue and the
inter blocks' DCT are added modulo 256, not clamped; the colours come from tables of terms each cut toward 0 (Y below 16
as 16): Y' = (Y - 16) x 1.16438, R = Y' + Cr x 1.596, G = Y' - Cb x 0.3918 - Cr x 0.813, B = Y' + Cb x 2.0173; the X
byte of the 32-bit surfaces is 0, as is the alpha of the alpha surfaces for a movie without an alpha plane. Its sound
(the games' tracks are all DCT, mono or stereo, 22050 to 48000 Hz) is `binkw32.dll`'s to within one step in the samples
(99.9 % of them equal; the rest lie on a half step, where the transforms' float sums part): the overlap of each block
with the last is faded on the samples, each rounded to 16 bits first, the sum shifted down by the overlap's count
(towards minus infinity).

## binkw32

The imports (`0x006CA32C` .. `0x006CA354`): `BinkOpenDirectSound`, `BinkSetSoundSystem`, `BinkSetVolume`,
`BinkPause`, `BinkNextFrame`, `BinkWait`, `BinkDoFrame`, `BinkSetSoundTrack`, `BinkCopyToBufferRect`, `BinkClose`,
`BinkOpen`. The code reads the BINK structure: Width `+0x00`, Height `+0x04`, Frames `+0x08`, FrameNum `+0x0C`,
NumTracks `+0xF0`.

## rad3d

Made at the first open (`0x0055B7C0`, 0x28 bytes, `[0x0075F1D8]` and `[0x0075F1E0]`): `+0x00` the IDirect3D9, `+0x04`
the device, `+0x08` `[0x0075F79C]+0x0C`. `0x0055AB80` checks the formats against the display mode's
(`GetAdapterDisplayMode`, `CheckDeviceFormat` of a texture of no usage) in this order:

| format | rad3d index | bytes | alpha |
|---|---|---|---|
| X8R8G8B8 | 0 | 4 | no |
| R8G8B8 | 8 | 3 | no |
| R5G6B5 | 3 | 2 | no |
| X1R5G5B5 | 2 | 2 | no |
| A8R8G8B8 | 1 | 4 | yes |
| A4R4G4B4 | 5 | 2 | yes |
| A1R5G5B5 | 4 | 2 | yes |

The first accepted one gives the opaque format (`+0x10`, index `+0x20`, bytes `+0x18`); the first accepted alpha one
the alpha format (`+0x0C`, `+0x1C`, `+0x14`); without either there is no rad3d. `+0x24` is 1 unless TextureCaps has
POW2 without NONPOW2CONDITIONAL. Then `BinkSetSoundSystem(BinkOpenDirectSound, [0x0075F858])` (the game's
DirectSound; OpenJuice passes 0 and Bink makes its own). `0x00559AA0` fills the Bink surface types by the index
(`[0x0075B324]`): 0 BINKSURFACE32 (3), 1 32A (5), 2 555 (9), 3 565 (10), 4 5551 (8), 5 4444 (7), 6 32R (4), 7 32RA
(6), 8 24 (1), 9 24R (2).

### Images

All movies share one image (`[0x0075F1BC]`), made for a size (`[0x0075F1C8]`, `[0x0075F1CC]`) and kept while the
movies opened are not larger; `[0x0075F1D0]`, `[0x0075F1D4]` are the size of the movie drawn. `0x0055B740` makes one
texture (`0x0055B6A0`) when both sides are powers of two (`0x0055AD20`: 4 .. 512, 1024 past it) or `+0x24` is set,
else tiles (`0x0055B060`, tiles of 0x100: whole tiles, then the rest across and down in tiles whose sides are made
powers of two). A texture (`0x0055AD80`) is `CreateTexture(w, h, 1, DYNAMIC, format, DEFAULT)`, else `(w, h, 1, 0,
format, MANAGED)`, and its rows zeroed. The image (0x50 bytes): `+0x04` format, `+0x08` index, `+0x0C` alpha,
`+0x10` bytes, `+0x14`, `+0x18` size, `+0x1C` the lock's place, `+0x20` one texture, `+0x24` the row bytes rounded
to 16, `+0x28` tiles, `+0x2C`, `+0x30` whole tiles across and down, `+0x34`, `+0x38` the last column's and row's
sides, `+0x3C`, `+0x40` the rest, `+0x44` the tile, `+0x48` the tiles' textures, `+0x4C` the texture.

A lock (`0x0055B5F0` one texture: the whole of it once, then no more; `0x0055B2F0` the tiles in turn with their place
in the frame and their size) gives the place the copy (`0x0055A580`) writes `BinkCopyToBufferRect(bink, bits, pitch,
h, 0, 0, x, y, w, h, the format's surface type | BINKCOPYALL when asked)` into, then the texture is unlocked. The
power-of-two sides of the last tiles are copied whole, past the frame's edge.

### The draw

`0x0055AF90` (the alpha): texture stage 0 COLOROP SELECTARG1; at an alpha of 0.9999 and more with an opaque image
ALPHABLENDENABLE 0, ALPHAOP SELECTARG1, COLORARG1 TEXTURE; else ALPHABLENDENABLE 1, SRCALPHA / INVSRCALPHA, ALPHAOP
MODULATE, COLORARG1 TEXTURE, COLORARG2 DIFFUSE; ALPHAARG1 TEXTURE. `0x0055AE40` draws a strip of two triangles (FVF
0x1C4, 0x20 bytes a vertex) of w x h texels scaled; the texture coordinates run to `[0x0075F1D0]` / `[0x0075F1C8]`
and `[0x0075F1D4]` / `[0x0075F1CC]` on every tile alike; diffuse and specular are white with the alpha x 255 (cut);
`[0x0075F144]` is 0 after. One texture: `0x0055B5A0`; tiles: `0x0055B3E0` row by row, each a tile x the scale on.

## The player

`0x0055A7C0` makes it (0x7C bytes, `0x0055A1D0`, vtable `0x006FA2F8`; its base `0x00559850`, vtable `0x006FA2D4`, a
named texture source kept by name in the texture manager's movies, `0x0055A960`). The open runs at once (a request
of the render thread, `0x0051CF00`). Fields: `+0x10` .. `+0x1C` the rectangle, `+0x20` played once, `+0x24` held,
`+0x2C` the name, `+0x30` the Bink, `+0x34` fast, `+0x40`, `+0x44` the scale, `+0x48`, `+0x4C` 0, `+0x50`, `+0x54`
the corner, `+0x58` the alpha (1), `+0x60` the volume, `+0x64` first, `+0x68` the sound track, `+0x6C` the file.

| vtable | function | what |
|---|---|---|
| `+0x00` | `0x0055A520` → `0x0055A6A0` | the draw: the movie's size the one drawn; the image at (width x `+0x48` x `+0x40` + `+0x50`, height x `+0x4C` x `+0x44` + `+0x54`), scaled, at the alpha; then the 3D renderer's material ended (`[0x0075F16C]`, `[0x00719540]` 0x10009) |
| `+0x04` | `0x0055A3C0` | the step (below) |
| `+0x08` | `0x0055A530` | finished: looped never; played once at FrameNum ≥ Frames, or without a Bink |
| `+0x0C` | `0x00559A90` | FrameNum, 0 without a Bink |
| `+0x10` | `0x00559A80` | `+0x34` |
| `+0x14` | `0x0055A790` | the volume `+0x60` and `BinkSetVolume(+0x68, 0x005B8B50(volume))`, the volume in hundredths of decibels (2000 log10, -10000 .. 0) |
| `+0x1C` | `0x0055A140` | (the close request) |
| `+0x24` | `0x0055A100` | the open again (`0x00559B10`) |

`0x00559A60` holds the player (`+0x24` 1) and steps it twice.

### The open (`0x00559B10`)

`<name>.bik` in the movies folder; without it a name ending in a digit tries the digit 1 and 2, the one of `rand() &
1` (1 + it) first; with none the refcount word gets 0x40000000: the player has no Bink, draws nothing and goes at
its first release. Unless `[0x0071E718] & 1`, the language's track (`[0x0075F79C]`'s method `+0x08`) is kept in
`+0x68` and asked for (`BinkSetSoundTrack(1, track)`); `BinkOpen(handle, BINKFILEHANDLE | BINKSNDTRACK)`; a movie of
one track is closed and opened again with track 0. With `& 1`: `BinkSetSoundTrack(0, 0)`. The rectangle gives `+0x40`
= its width / the movie's, `+0x44` the same down, `+0x50`, `+0x54` its corner cut to whole pixels; `+0x34` 0, `+0x58`
1, `+0x48`, `+0x4C`, `+0x5C` 0, `+0x60` 1, `[0x0075F1C4]` 0, `+0x64` 1. The image is made again when the movie is
larger than it; `[0x0075F1C0]` counts the movies open.

### The step (`0x0055A3C0`)

With the window inactive (`[0x0075F780]` 0): the volume 0, nothing more. Unless held the volume (`+0x60` x 32768,
cut); without a Bink nothing more; held: the volume 0. Otherwise the volume again and:

- fast (`+0x34`): the next frame asked for (`BinkNextFrame`) when FrameNum ≤ 1 or `BinkWait` says its time has come;
  then `BinkDoFrame` and the copy. A branch for `[0x0075F1C4]` (the first frame decoded, the movie paused at frame
  0 and drawn) never runs: the value is only written 0.
- else: at frame 1 `BinkWait` is spun until the time comes; then with the time come `BinkDoFrame`, the copy of the
  whole frame and `BinkNextFrame`, else the frame copied again.

A movie played once at FrameNum ≥ Frames is held and silent (the step returns 0).

### The release

`0x0055A300` sets the volume of its track to 0; the close (`0x0055A140`): with a Bink the volume 0 and `BinkClose`,
one movie open less and with none left the image freed; the name leaves the texture manager (`0x005599B0`).

## The interface's movie

`[0x0075F838]` the player, `[0x0075DBD8]` its name, `[0x0075F834]` the element that drew it last (never read),
`[0x0075F840]` set by a draw (never read), `[0x0075F064]` the frame's hold: set when a folder of the game is opened
(`0x004F9970` makes a folder object, `0x004FCA50`), cleared with `[0x0075F840]` at the frame's start (`0x0040CCD9`);
while set no movie is opened or stepped. `0x005AA340` gives the movie back; `0x005AA300`, `0x005AA3A0` and
`0x005AA4C0` are the element's (`04-interface.md`).

OpenJuice opens no folders of its own; the hold is set by nothing.
