# Upstream source integration

The native macOS branch includes the upstream retail/1.01/dx9 snapshot through
`ceba8f88f891173532978b9686705f629ba288df` (OpenJuice 1.01.4, 2026-10-22). The previous imported snapshot was
`2805fd213`. Upstream release branches are rebuilt, so this integration uses
the complete snapshot difference rather than merging unrelated repository roots.

Source: https://gitlab.com/sidecr3w/openjuice/-/tree/retail/1.01/dx9
Releases: https://gitlab.com/sidecr3w/openjuice/-/releases

Includes 1.01.3 and 1.01.4 gameplay, menu, audio, rendering, Windows, documentation
and regression-test changes. CHANGELOG.md contains upstream's detailed release notes.
The hosted-event correction complements the native migration for older saved events;
the latter remains in place. Added-car IDs/save extensions, native controls, paint
shortcut and Metal reflection corrections remain intact.

Native adaptations: COM initialization is Windows-only in the background loader.
Metal serializes shared resource maps against rendering and submits background
uploads using command buffers acquired on the worker thread, as SDL GPU requires.
The main thread can yield its frame lock while waiting for an engine loading lock.
The Windows-only DirectX check unit test is retained in source but excluded from
the macOS CMake target; the sound test uses the native backend without a Windows
desktop handle.
