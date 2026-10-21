# The .dat archives

The shipped game keeps most of its folders packed: `carmesh.dat`, `carmodels.dat`, `cartextures.dat`, `effects.dat`,
`mesh.dat`, `models.dat`, `scripts.dat`, `textures.dat` and `ui.dat` in the game's folder, a `<car>.dat` and a
`<car>_ui.dat` for each car in `cars\`, a `<track>.dat` for each track in `tracks\` (125 archives, 27875 files).
Unpacked installs rename them away (`_scripts.dat`) and keep the folders. `replay.dat`, `steering.dat` and
`cars\_ui.dat` are not archives.

## Mounting (`0x004F9970`)

The game opens its folders as file systems (slot 0 opens a file by name, slot 1 its size). Opening the folder
`<name>` in a parent: `<name>.dat` is looked for in the parent (its slot 1); when it is there and reads as an archive
(`0x004FCEA0`) the folder is that archive, else a folder of the parent (`0x004FC2C0`). The folders under an archive's
folder are paths inside the archive.

Opening a file in an archive (`0x004FCDA0`): the name's hash is looked for in its table; a name the archive does not
hold is opened in the parent as `<the archive's name without .dat>\<name>`, so loose files beside a packed folder are
still read. The archive's size (`0x004FD010`) looks inside the archive only: a file only loose there is not seen.

## The file

- `+0x00` the count of files, `+0x04` the offset of the table.
- The table: an entry of 20 bytes a file: the name's hash, the offset of its chunk list, the count of chunks, the
  packed size, the size.
- The chunk list: `{offset, size}` a chunk, each a zlib stream (RFC 1950); the file is the chunks inflated in order.

The names are the paths inside the archive in lower case with `\` between the folders (`mesh\eclipse_bonnet.d8m` in
`cars\eclipse.dat`, `cardefs.txt` in `scripts.dat`). The hash (`0x004FCF80`): for a name of n bytes c (signed),
the sum over i < n - 1 and j from i to n - 1 of (c[j] + j)(c[j] + 19)(c[i] + 7)(c[i] + i), in 32 bits, mod
4000000000; 0 for an empty name.

## OpenJuice

`Core/Formats/DatArchive` reads an archive's table once and inflates a file's chunks as it is read (`Core/Formats/Inflate`, its own
zlib); `Core/Base/GameFiles` mounts as the game does: going down a path, the first folder with a `<folder>.dat` beside it
is that archive, the rest of the path the name inside; `read` falls back to the loose file, `exists` looks inside the
archive only. The text tables and the interface's `.ui` files are read through it too. The audio and the movies are
never packed and are read from their folders.
