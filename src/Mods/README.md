# Creative mods (optional macOS module)

Enable with `--creative-mods`. The ordinary game stays on the stock code paths when
this flag is absent. `--mods-menu` also opens the image menu at startup for testing.
Menus reuse the retail `opts_bg`, `opts_menu`, and `msgbox` widgets. AppKit is used
only for selecting image files; ImageIO makes bounded 512px native textures.

## In the game

- **Options → Mod Garage**: choose/change/remove a driver photo or crew logo.
  The first crew-logo choice becomes the imported logo; importing from Options
  also selects it. PNG transparency is preserved, JPEG is supported, portraits
  are square-cropped, and logos are fitted without stretching.
- Creating or first loading a driver asks for a photo once. Continue skips it.
- **Garage → Styling → Neon → Change → Custom RGB + Effects**: left/right changes
  RGB channels by one; Enter adds ten. Choose Glow, Pulsating, Back and Forth, or
  RGB Cycle and speed 1–5. Apply uses the normal 1,000-credit purchase confirmation
  and the original unlock rules. Back and Forth moves a glow band under the car.
  Buying a standard palette colour resets the effect to Glow.
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

The implementation lives in `src/Mods`. Small guarded hooks handle menu requests,
profile changes, native image loading, decal textures and neon drawing. The
renderer exposes a read-only texture accessor so temporarily replaced decal
textures can be restored. This is not a general third-party code/plugin loader.

## Checks

`mac_creative_check` tests import, persistence, transparent padding, channel order,
profile isolation, editor apply/cancel, animation and path confinement. Supplying
`GAME MOD` additionally fits custom images to every available car's decal material
and verifies restoration after changing back to the original decal.
