# Optional mod bundle

```sh
python3 tools/mod_bundle/assemble.py \
  --ferrari ~/JuicedMods/Ferrari458-Added \
  --wheels ~/JuicedMods/UltimateWheels \
  --output ~/JuicedMods/PolishedGarage
```

Run the native game with `--mod ~/JuicedMods/PolishedGarage --saves '/path/to/separate/saves'`.

Assembly preserves the newest Ferrari car definitions, adds wheel colours, and takes the wheel pack's appended catalogue. Large binary assets use hard links on the same filesystem. Text files are copied, and no retail archives change. The original overlay directories should be treated as immutable; updates should generate fresh directories and a fresh bundle. Building the bundle takes little additional disk space, but copying it into an application may require about 500 MB.

The output must be absent or empty. Existing saves are never copied into a bundle. These scripts and the source repository contain no extracted wheel-pack or retail assets.

## Package-based app (current)

The independent packages now live in `mods/`. They are loaded and combined by the
native host at startup; the legacy assembly command above is only for old overlays.

```sh
python3 tools/mod_bundle/install_macos.py \
  --base-app /Applications/Juiced.app --exe out/juiced \
  --output '/Applications/Juiced Mod Garage.app' --creative-mods --replace
```

Do not pass `--mod` with `--creative-mods`. Each package's assets and Lua script are
bundled; the launcher copies missing files into `~/JuicedMods/mods`, preserving
edits and toggle markers. The game builds its selected overlay without external
build tools. See [packages and toggles](../../mods/README.md).

`package_overlay.py` converts a freshly validated legacy overlay into assets and
retail-script recipes. Use a fresh output package and copy its `mod.ini` separately.
For wheels, `--wheel-colours` extracts only the wheel-colour additions, avoiding a
dependency on whichever car overlay was used during the original import.

The generic retail reader lives in `tools/mod_bundle/retail.py`. Ferrari and wheel
authoring tools live in their own package's `tools/` folder. They are excluded from
the installed app, which only needs manifests, scripts, assets and recipes.
