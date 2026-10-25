# Modern Controls

`main.lua` owns the default driving bindings and the paint purchase shortcut (0).
Change the Lua values and restart; no rebuild is needed. Saved bindings in the
native Controls editor override package defaults. Invalid or conflicting defaults
fall back to the host's valid defaults.

Toggle this package in **Options → Mods**, then restart. When disabled, retail
input maps are used and the modern Controls editor is hidden. Existing custom
bindings remain on disk and return when the package is enabled again.

The engine provides key capture, conflict swapping, native menu widgets and
settings persistence as host services; Lua supplies the optional policy.
