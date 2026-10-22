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

Package a separate Mac application without duplicating the large assets:

```sh
python3 tools/mod_bundle/install_macos.py \
  --base-app /Applications/Juiced.app --exe out/juiced \
  --mod ~/JuicedMods/PolishedGarage \
  --output '/Applications/Juiced Mod Garage.app' --creative-mods
```

Only pass `--creative-mods` with a build implementing that optional feature. The launcher uses separate `Juiced Mods/ModGarage` saves and does not open the mod menu automatically. The app's asset hardlinks remain valid if source overlays are deleted, but editing source assets in place also edits the app; rebuild into a fresh directory instead.

With `--creative-mods`, the packager includes the runtime Lua interpreter build and
copies `mods/` (or `--scripts PATH`) as bundled defaults. On first launch the app
seeds `~/JuicedMods/mods`, then loads scripts from there with `--mods`. Existing
user scripts are preserved when rebuilding or reinstalling the app. Edit scripts
and restart the game to apply changes; no compilation is needed. See
[the runtime API](../../mods/README.md).
