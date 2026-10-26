# Runtime mods — API 1

These are **Lua source scripts loaded by the running executable**, not C++ compiled
into it. Edit a script, quit and relaunch: no compiler, build tools, Homebrew or
Lua installation is needed. The interpreter is linked into the game.

The installed Mod Garage launcher seeds `~/JuicedMods/mods` from its bundled defaults
on first launch, then always uses that editable folder. Reinstalling the application
does not overwrite existing scripts. The repository's `mods` folder is the source
of those defaults. Direct launch:

```
out/juiced --game /path/to/Juiced --mods /path/to/mods
```

Each script package contains `main.lua` returning `{api=1, ...callbacks...}`. A file named
`disabled` inside that subfolder disables it. Loose `.lua` files are also accepted.
Files load in sorted path order. A hook returning `nil` leaves the previous result
unchanged; the last non-nil result wins. Unknown API versions and scripts with
syntax errors are skipped. A callback error disables that script for the session.
Errors and loaded script names appear in `~/Library/Application Support/Juiced/Juiced.log`.

## Packages and in-game toggles

Every shipped mod has its own folder and `mod.ini`: `controls`, `neon`,
`driver-photo`, `custom-logo`, `ferrari`, `wheels`, and `host-pink-slips`. Script packages contain
`main.lua`; asset packages contain `assets/`, `patches/`, and their authoring tools.
All seven packages, including ready-to-load car/wheel assets, are committed here.

Open **Options → Mods**, select a mod and press Enter to turn it on/off. An asterisk
marks a change pending restart. Toggles are saved immediately; Back closes the menu.
Restart to apply them. Menus reuse Juiced's existing widgets, font and highlight.
Driver photo/logo editing remains under **Options → Mod Garage** when enabled.

A `disabled` marker in a package controls its next launch. The running catalogue,
input maps and Lua callbacks stay unchanged until restart. Disabling controls
restores retail input maps; saved bindings are retained. Disabling neon restores
the stock effect; saved custom effect settings are retained.

Car, wheel and custom-logo packages change saved catalogue indices. Different
combinations use different career directories under `variants/`. The complete
original set retains the existing ModGarage careers, as declared by
`collection.ini`. Re-enabling that set resumes those careers. Photo, neon and
controls and host-pink-slips toggles do not split saves. No existing career is converted or deleted.

The launcher copies missing package files into `~/JuicedMods/mods`, preserving
existing scripts and enable/disable choices. This is the editable installed copy;
this repository holds its defaults. Reinstalling does not overwrite edits.

The native package host builds a temporary asset overlay at startup, using
hardlinks where possible. Recipes copy byte ranges from original retail scripts
and insert package changes; a fingerprint guards against incompatible data.
Wheel colours append to the chosen car catalogue. Conflicting replacements fail
explicitly. Successful sessions remove their temporary overlay on exit; interrupted
sessions can leave expendable `.cache/stage-*` directories. Retail files are never
modified. No Python, Node or compiler is needed to play.

The engine still supplies shared input, image-import, rendering, catalogue and UI
services in C++. These are host APIs, not arbitrary native-code injection. Changing
Lua policy or packaged assets needs a restart, not recompilation; adding a new
engine capability requires extending the host API.

## Bundled scripts

- `controls/main.lua`: default bindings and paint-purchase key/label.
- `neon/main.lua`: effect names/count, colour calculations and animated texture
  selection. Add an effect to the names table and handle its zero-based index in
  `neon_colour`/`neon_texture`; the picker discovers it at runtime. Keep old indices
  stable because existing saves use them.
- `driver-photo/main.lua`: first-profile prompt policy, photo menu entries and
  portrait texture substitution.
- `custom-logo/main.lua`: decal-slot registration, logo menu entries and crew-logo
  texture substitution.

## Host callbacks

| Function returned by a mod | Arguments | Result |
| --- | --- | --- |
| `feature` | name: `controls`, `neon`, `portrait`, `logo` | true to activate that host facility; nil for unrelated names |
| `control_default` | action ID from the controls host | `DIKEYBOARD_*` key name |
| `paint_buy_key` | none | ASCII character (48 for 0) |
| `paint_buy_label` | none | shortcut label |
| `effect_count` | none | 1–64 |
| `effect_name` | zero-based effect index | display string |
| `neon_colour` | RGB integer, effect, speed, seconds | RGB integer 0–16777215 |
| `neon_texture` | RGB integer, effect, speed, seconds | `mod_neon_glow` or `mod_neon_scan_0` through `_31`; nil for fallback |
| `profile_prompt` | already asked, has photo | boolean |
| `image_alias` | original texture name, portrait texture name, logo texture name | replacement name or nil |
| `logo_slot` | car model name, part count, existing custom-logo index (-1 if absent) | existing index, count to append, or -1 to skip |
| `image_menu` | has portrait, has logo, onboarding | array of `{label, help, action}` records |

Image actions exposed by the native host: `import_portrait`, `import_logo`,
`clear_portrait`, `clear_logo`. Menu records from different mods are combined.
The host appends Back/Continue. PNG/JPEG selection and bounded texture decoding,
rendering, purchases, input and persistent storage remain native engine services.

Lua gets base, math, string, table and utf8 libraries. Filesystem/shell/package/debug
APIs are not exposed. Each script has a 16 MiB Lua allocation limit, callbacks have
an instruction limit, and malformed results fall back to the host defaults.
API tables and menu records use raw fields: `__index` is not invoked by the host.
`pcall`/`xpcall` can catch ordinary Lua errors, but execution-budget exhaustion
always disables the mod. `setmetatable` rejects `__gc` and `__close` finalizers,
which otherwise can execute outside normal callback protection.
Install scripts you trust; this is not an OS security boundary.

This is a versioned scripting interface, **not arbitrary binary patching**. Mods
can change behaviour exposed through these hooks without recompilation. Adding an
entirely new engine capability still requires extending the native API once.
There is no automatic mid-session reload: restart after changing scripts so car
catalogues and active menus remain consistent.

Host Pink Slip Races adds an optional type to career event setup. Select a rival
with at least 600 respect and compatible cars; the original 1v1 car-wager flow
settles the result. Requires a native build containing this hook (Android .7+).
See [package instructions](host-pink-slips/README.md).
