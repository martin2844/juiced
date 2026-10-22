# Runtime mods — API 1

These are **Lua source scripts loaded by the running executable**, not C++ compiled
into it. Edit a script, quit and relaunch: no compiler, build tools, Homebrew or
Lua installation is needed. The interpreter is linked into the game.

The installed Mod Garage launcher seeds `~/JuicedMods/mods` from its bundled defaults
on first launch, then always uses that editable folder. Reinstalling the application
does not overwrite existing scripts. The repository's `mods` folder is the source
of those defaults. Direct launch:

```
out/juiced --game /path/to/Juiced --mod /path/to/assets --mods /path/to/mods
```

Each subfolder contains `main.lua` returning `{api=1, ...callbacks...}`. A file named
`disabled` inside that subfolder disables it. Loose `.lua` files are also accepted.
Files load in sorted path order. A hook returning `nil` leaves the previous result
unchanged; the last non-nil result wins. Unknown API versions and scripts with
syntax errors are skipped. A callback error disables that script for the session.
Errors and loaded script names appear in `~/Library/Application Support/Juiced/Juiced.log`.

## Bundled scripts

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
| `feature` | name: `neon`, `portrait`, `logo` | true to activate that host facility; nil for unrelated names |
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
Install scripts you trust; this is not an OS security boundary.

This is a versioned scripting interface, **not arbitrary binary patching**. Mods
can change behaviour exposed through these hooks without recompilation. Adding an
entirely new engine capability still requires extending the native API once.
There is no automatic mid-session reload: restart after changing scripts so car
catalogues and active menus remain consistent.
