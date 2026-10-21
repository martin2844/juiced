# Atmospheres and the sky dome

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Files

A 3D element's `Atmosphere <name>` and the career map read `ui\atmospheres\<name>.atm` (`0x005B4610` →
`0x00531510` → `0x00533EC0`); a track's conditions read `<track>_<time>_<weather>.atm`. The file is a script
(`05-scripts.md`) with the keys of the table at `0x007244B0` (16-byte entries, filled by `0x006C28A0`).
`[BeginAtmosphere]` (`0x00531E30`) makes a new atmosphere object (0x1C8 bytes) with the defaults of `0x00531D00`, its
name the next word when it is shorter than 32 characters, puts it in the set's next of 20 slots (`+0x08`, the count
`+0x60`) and makes it the one in use; the other keys write into it. Once the file is read the set's first atmosphere is
the one in use (`0x0053393B`).

| key | values | field |
|---|---|---|
| `FogColour` | r g b | `+0x94` and `+0x98` |
| `FogColourFront`, `FogColourBack` | r g b | `+0x94`, `+0x98` |
| `FogMin`, `FogMax`, `FogNear`, `FogFar` | float | `+0x9C`, `+0xA0`, `+0xA4`, `+0xA8` |
| `AmbientColour` | 3 floats | `+0x20` |
| `LightColour1` .. `3` | 3 floats | `+0x30`, `+0x40`, `+0x50` |
| `LightDirection1` .. `3` | elevation azimuth (degrees) | `+0x60`, `+0x70`, `+0x80` |
| `SkyColour` | r g b | `+0x90` |
| `SkyAngle` | degrees | `+0xAC` (× π/180 as `0x3C8EFA35`) |
| `HorizonColourFront`, `HorizonColourBack` | r g b | `+0xB0`, `+0xB4` |
| `SkyColourFront`, `SkyColourBack` | r g b | `+0xB8`, `+0xBC` |
| `ZenithColour` | r g b | `+0xC0` |
| `SkyHeightFactor`, `SkyHeightManual` | float | `+0xC4`, `+0xC8` |
| `SunShape`, `SunFlare`, `SunReflection`, `Envmap` | name (32 characters) | `+0xCC`, `+0xEC`, `+0x10C`, `+0x12C` |
| `SunSize`, `SunFlareSize`, `SunReflectionSize` | float | `+0x14C`, `+0x150`, `+0x154` |
| `SkyLuminosity` | float | `+0x158` |
| `SunAngle` | degrees | `+0x15C` |
| `ExposureEnable`, `ExposureBias` | int, float | `+0x160`, `+0x164` |
| `ExposureLimits` | max min | `+0x16C`, `+0x168` |
| `Night`, `Wet` | — | `+0x170`, `+0x174` = 1 |
| `LightBlur` | float | `+0x17C` |
| `ShadowSet` | int | `+0x178` |
| `WorldShadowColour` | r g b a | `+0x180` |
| `WorldShadowAdditive` | — | `+0x184` = 1 |
| `NightRoadShininess` | float | `+0x188` |
| `MinRoadWetness` | float | `+0x18C` and `+0x190` |
| `MaxRoadWetness` | float | `+0x194` |
| `RoadWetnessTime` | seconds | `+0x198` = 1 / value |
| `MinRainOnTime`, `MaxRainOnTime`, `MinRainOffTime`, `MaxRainOffTime` | float | `+0x1A8` .. `+0x1B4`; the timer (below) |
| `PuddleColour`, `PS2BuildingGlassColour` | r g b | `+0x1B8`, `+0x1BC` |
| `TestMipmapBias` | float | `+0x1C4` |
| `[EndAtmosphere]` | — | nothing |

A colour is `0xFF` and the low bytes of `atol` of the three words (`0xAARRGGBB`; `WorldShadowColour` takes the fourth
word as alpha). A direction (`0x005324D0`): e = elevation × `0x3C8EF921` rounded to a float, a = azimuth (as `atof`
gives it) × the same, then (sin a cos e, −sin e, cos a cos e). A missing word reads as 0.

Defaults (`0x00531D00`, `0x00531E30`): SkyColour white, FogNear 0, FogFar 400 000, horizon 255 247 255 front and
218 227 233 back, sky colours 0, zenith 55 126 190, SkyAngle 0, SkyHeightFactor 4, SkyHeightManual 500, names
empty, SunSize 0, SunReflectionSize 100, SkyLuminosity 0.25, SunAngle 0, ExposureEnable 1, ExposureBias 0, limits
128 / −128, Night and Wet 0, ShadowSet −1, LightBlur 0.5, WorldShadowColour 255 255 255 200, NightRoadShininess
0.25, road wetness 1, `+0x19C` 1, `+0x1A0` 1, the rain times FLT_MAX, PuddleColour white, TestMipmapBias 0.

Loading into a world (`0x00531510`) also sets the world's light 1 direction to the atmosphere's (`0x005341F0`) and
its environment texture (`0x0054DBC0`): `textures\<Envmap>.dds`, `arcade_env` without one, a cube texture made by
D3DX with its default mip levels. The interface's worlds draw with it (`0x0054C5C0`); the race's world draws with its
reflection cubes instead (`15-tracks.md`).

## The rain

`+0x1A0` is set while it rains (1 at first), `+0x1A4` the time left until it stops or starts (FLT_MAX at first) and
`+0x19C` how much it rains (1 at first). Each rain time, as it is read (`0x00533130` .. `0x00533280`), brings the timer
of the state it belongs to within it: while raining `MinRainOnTime` raises it to the time when the time is greater,
`MaxRainOnTime` lowers it when the time is less; while dry the same with the off times (the value as `atof` gives it
against the timer).

Every frame of a race, after the physics, with its step time (`0x00534300`, from `0x00425D40` on the world's current
atmosphere): the timer falls by the time; below 0 the rain starts or stops and the timer is drawn between the new
state's minimum and maximum: (max − min) × (rand() × 1/32767) + min. While raining `+0x19C` rises by half the time up
to 1, while dry it falls by it down to 0. The road's wetness (`+0x18C`) grows by `+0x19C` × the wetness rate × the
time, within the minimum and the maximum wetness.

## Every frame (`0x00533B80`)

With an atmosphere the world takes its values instead of its own lights (`0x0054DC90`):

- The fog colour: h = atan2(z of the camera's X axis row, z of its Z axis row) (`world[8]`, `world[10]`),
  f = |wrap(h + SkyAngle) × 1/π| (wrap into (−π, π]); fog = mix(front, back, f) with mix (`0x00531B60`) each
  channel round(front × f + back × (1 − f)).
- In order: ambient (c25), light 1 colour (c26), light 2 colour (c28), light 3 colour, the three at once (c26 .. c28),
  light 1 direction (c27 ..), light 2 direction (c29), light 3 direction (c31), the fog colour (FOGCOLOR, c50),
  FogMin / 255, FogMax / 255, FogNear, FogFar each followed by the density (c51).
- The renderer keeps the zenith colour (`+0x168`), LightBlur (`+0x16C`), NightRoadShininess (`+0x174`), `+0x18C`
  (`+0x178`), `+0x1C0` (`+0x188`; 1 for the atmosphere the set's loading puts in use, `0x00533950`, which also sets the renderer's `+0x188` to 1) and the night flag (`+0x38`, vtable `+0x20`); WorldShadowColour and
  WorldShadowAdditive go to vtable slots that do nothing.

While the conditions change, each value is blended with the next atmosphere: colours by mix(next, current, t),
numbers as (1 − t) × current + t × next.

### Atmosphere zones

A track's `atmos*` helper among the roots is an atmosphere zone (`0x00418B50`, `21-scene-objects.md`). At the end of
the track's making (`0x00422AC0`) each takes the set's atmosphere of its name (`0x00533A70`: the first of the 20 slots
whose name is the zone's, any case), none without one. Each frame the race camera (`0x00406300` → `0x00422C40`) finds
the zone nearest its place in the world (distance `sqrtf((z² + y²) + x²)`, the first of equal ones); within that zone's
radius (`0x00418BF0`) the set's first atmosphere is in use (`+0x58`), the zone's is the next (`+0x5C`) and t
(`+0x64`) is 1 up to 0.75 of the radius, then 1 − (d − 0.75 r) / (r − 0.75 r). Outside every radius nothing is set. The
game's tracks have no `atmos*` helper.

## The sky dome

The career map (`0x005B4660`, after its sun: `21-scene-objects.md`) and a track build the dome of the atmosphere in use
(`0x005315A0` → `0x0054D030` → `0x005530B0`); without an atmosphere: horizon and sky colours 255 247 255 / 218 227 233,
zenith 55 126 190, angle 0, height factor 0, luminosity 0.25, height −50.

- Rings i = 0 .. 8: a = −(π/2 × i/8); y = −sin a × 5000, radius cos a × 5000; height h = 1 − i/8, e = 1 for h = 0,
  else 1 − h^SkyHeightFactor (`pow`).
- A band (rings i and i + 1) is 33 pairs of vertices j = 0 .. 32: t = j/32, f = 2t up to 0.5 and 2 − 2t after;
  horizon = HorizonFront × (1 − f) + HorizonBack × f, sky = SkyFront × (1 − f) + SkyBack × f; a ring's colour with
  e ≥ 0.5: w = 2(e − 0.5), zenith × w + sky × (1 − w), else w = 2e, sky × w + horizon × (1 − w). Angle = t × 2π +
  SkyAngle + π: x = radius × sin, z = cos (as a float) × radius, y = the ring's y − 100. After each band its last
  vertex twice more; 8 bands, 544 vertices. A sky colour of 0 is the zenith and the horizon colour halved (alpha
  128, `0x00553BE0`).
- The vertex buffer (`0x0054D030`) holds position and D3DCOLOR: each channel rounded, alpha = round(SkyLuminosity ×
  255); the material is class 41 Sky (setup `0x00528560`: CULLMODE the default and vertex c52 = (256, 256, 256, 1)
  through the state manager; drawing `0x0052B840`: the program's pass).
- Drawing (`0x0054D510`, the world's draw after the lights and before the scene): the Sky material's pass, model =
  the identity at (camera x, camera y − SkyHeightManual, camera z): c0 and c4 the model (3 rows), c18 world-to-clip ×
  model; FOGENABLE off, a triangle strip of 542 triangles, FOGENABLE on.
