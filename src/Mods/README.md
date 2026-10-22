# Creative mods (optional macOS module)

The native host loads Lua scripts from `--mods /path/to/mods`. See
[the runtime API](../../mods/README.md) for installing, disabling and editing mods.
`--creative-mods` is a compatibility shorthand for loading
`~/Library/Application Support/Juiced/mods`; the installed Mod Garage app explicitly
loads `~/JuicedMods/mods`. `--mods-menu` also opens the image menu for testing.

Menus reuse retail widgets. AppKit is used only for selecting image files;
ImageIO makes bounded 512px textures. Feature policies, image menu entries,
texture aliases and neon animation formulas live in external scripts.

## In the game

- **Options → Mod Garage**: choose/change/remove a driver photo or crew logo.
  The first crew-logo choice becomes the imported logo; importing from Options
  also selects it. PNG transparency is preserved, JPEG is supported, portraits
  are square-cropped, and logos are fitted without stretching.
- Creating or first loading a driver asks for a photo once. Continue skips it.
- **Garage → Styling → Neon → Change → Custom RGB + Effects** opens the existing
  paint colour chart with the car and garage still visible. Arrow keys select the
  colour, **Q** cycles effects, **E** cycles speed, and the underglow previews live.
  **Enter** opens the standard 1,000-credit confirmation; **Esc** restores the
  previous neon. Controller SELECT2/SELECT3 also cycle effects/speed. The original
  unlock rules apply. Standard palette purchases reset the effect to Glow.
- **Garage → Styling → Decals → Custom Logo**: uses the imported image. Selecting
  this without an image opens the import menu. The Ferrari has fitted bonnet and
  door shells. Original cars reuse their native prototype-decal geometry/UVs, so
  the picture's placement follows each car's original layout.

## Storage and boundaries

`CreativeMods/<driver-name-hash>/creative.ini` and `images/*.ddt` are stored beside
that launch's save directory. The bundled app uses
`~/Library/Application Support/Juiced Mods/ModGarage/CreativeMods`.
Image names are immutable unique keys, so changing photos doesn't reuse stale GPU
textures. Retail archives, source pictures and original profile files are not
modified by the image importer. Settings use temporary files and atomic rename.

Effect preferences are per driver and car model; they apply when that model's
fitted neon matches the chosen RGB. Two owned copies of the same model with the
same RGB share an effect preference. Colour and the purchased neon part remain
in the game's own car save. Damage, daylight fading and neon failures still apply.

Custom-logo choices are appended, preserving every existing saved decal index.
Crew livery selection and automatic modification use the original part count and
never pick personal artwork. Saves containing these appended choices should stay
with the mod-enabled installation. Images aren't embedded into replays or sent
through multiplayer.

The host implementation lives in `src/Mods`, with small guarded hooks at the
engine boundaries. Scripts live outside the executable in `mods`. Persistence,
image decoding and rendering remain native services. This is an API-based Lua
mod system, not arbitrary executable patching.

## Checks

`mac_creative_check` tests import, persistence, transparent padding, channel order,
profile isolation, preview rollback, animation and path confinement. Supplying
`GAME MOD` additionally fits custom images to every available car's decal material
and verifies restoration after changing back to the original decal.

`mac_mod_runtime_check` verifies script edits, disable markers, error isolation and
execution limits. `mac_neon_picker_check GAME MOD [capture.bmp]` exercises the real
paint chart, live effects, cancellation and native purchase with game assets.
