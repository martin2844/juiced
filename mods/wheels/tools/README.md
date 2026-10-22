# Eagle Ultimate Wheels (optional local import)

Installs the user's `Juiced_Ultimate_Wheels_Mod_Eagle.rar` as loose assets. No game archives or source assets are redistributed or overwritten. Credits and original instructions remain in the generated `EAGLE-README.txt`.

```sh
python3 mods/wheels/tools/install.py \
  --archive ~/Downloads/Juiced_Ultimate_Wheels_Mod_Eagle.rar \
  --game ~/JuicedData \
  --base-overlay ~/JuicedMods/Ferrari458-Added \
  --output ~/JuicedMods/UltimateWheels
```

This pack adds 128 wheel entries after the original 74, preserving saved wheel indices and each car's factory wheels. It also adds 40 unique wheel colours. The importer supplies missing vertex colour and secondary UV declarations required by Metal, avoiding a GPU pipeline crash when selecting imported wheels. Each normal and blurred wheel mesh, its materials and texture dependencies are checked during import. The native garage and tyre catalogue stay unchanged. Find the new wheels through the normal garage wheel suppliers (including Falken, BBS, Enkei and the other supplier categories named by the archive).

An existing nonempty output is rejected. Use a new output for reinstallation; do not reuse saves after removing the wheel catalogue if cars have imported wheels fitted.

To combine with the latest Ferrari overlay, run `tools/mod_bundle/assemble.py`. The wheel import's copy of `cardefs.txt` is not a standalone car asset installation; use the assembled bundle when it was generated against a Ferrari overlay.
