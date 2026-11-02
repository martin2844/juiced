# Ten-car RP5 playtest delivery — 2026-10-31

The user requested an immediate RP5 delivery instead of further evaluations:
“creo que basta de evals, no podemos ya meterlos en la RP5 y yo pruebo?”.
This supersedes the previous restriction on using the device for this import.
Stop automated visual/handling evaluations and remote gameplay after handing it
back. User playtest feedback is the next input; full acceptance is not claimed.

## Delivery

`out/releases/OpenJuice-Car-Visual-Fixes-r16.zip` contains Ferrari revision4,
Street Cars revision7 and Wave2 revision14. It includes the ten new cars,
existing catalogue identities and previous r15 fixes. New changes are24 existing
first-wave Sport1 meshes and56 Wave2 kit1 meshes, plus4 Outback kit2 skirt UV
corrections. No additional hidden mechanical engine modeling was undertaken.
“Native engine captures” refers to the game renderer.

Archive:190,012,118 bytes;3,056 files. SHA256:
`a4587b26477314a84d063b0ab36d5e1a9d1ae0ccd9c5921759668b70b0aaa585`.
The device received that exact hash through USB and normal launcher ZIP import.
The launcher returned `OK: 64 cars loaded. Data and active mods validated.`
APK19 remains installed; no APK uninstall, data clearing or save restoration.
Linux also uses the exact r16 runtime payloads through the habitual launcher.
The two updated packages were exchanged with retained r15 rollback copies;
repository runtime defaults match the installed payloads. No additional visual
or handling evaluations were run for this synchronization.

Normal launcher save exports before and after importing the pack contain the
same nine entries, each with identical SHA256. Ronnie's profile matches the
previous native inspection that contains all ten imports, plus Ferrari.
Private backups remain under `out/validation/rp5-cars-r16-20261031` and in the
device Downloads directory. Do not publish these saves.

## Evidence and limits

Deployment preparation, ZIP checks, USB hash, launcher screenshots and save
preservation receipt are in `out/validation/rp5-cars-r16-20261031`.
Native candidate checks before the request:1,238 Wave2 and483 first-wave
assertions on the complete64-car composition. First-wave opaque-centroid side
ray samples report zero newly hidden/floating cases across kits1/2/3 after the
Sport1 correction; this is a finite sample, not every raster pixel.

The private additional geometry verifier stopped at a global minimum-vinyl-Y
bound for the first-wave rear bumper. That conservative Wave2 condition is not
established for the first-wave fitted additions; it is retained as incomplete
evidence, not silently counted as passing. No further evaluations were run after
the user's request. Remaining uninspected views, exact factory handling variant
questions and physical gameplay approval remain open. This is a delivered
playtest checkpoint, not a claim that all ten ports are fully accepted.

Editable studies and candidates are retained at
`/run/media/martin/Ediloco/OpenJuice-builds/20261031-wave2-kit1`.
The ten editable Blender/GLB studies are also preserved in the repository:
`tools/street-cars-wave2/assets/kit1-20261031` (seven cars) and
`mods/street-cars/tools/assets/sport1-20261031` (first three cars), with the exact
authoring script versions and source reports. See `editable-source-receipt.json`
and `linux-r16-delivery.json` in the deployment evidence directory.
The previous Linux r15 ZIP/checkpoint remain available for rollback. No public
release or Git push was performed in this delivery.
