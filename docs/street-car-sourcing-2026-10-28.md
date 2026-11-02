# Street car sourcing — 2026-10-28

The earlier requirement that the user download models is superseded. Three actual
GLB files have now been acquired from the public redistribution library
https://github.com/pedrosoandre/carros (assets directory). Original creator metadata
was independently retrieved from the public Sketchfab API; all three currently
specify CC BY 4.0. Preserve original credits and identify modifications when shipping.

| Candidate | Creator | Downloaded triangles | Size | Assessment |
| --- | --- | ---: | ---: | --- |
| BMW E36 M3 | Martin Trafas | 38,979 | 2,543,508 bytes | Detailed tuned street coupe; lowered stance and aftermarket wheels need deliberate handling. |
| Honda Integra DB8 Type-R | Myedsu / Lexyc16 | 15,452 | 2,122,448 bytes | Strong first pilot: reasonable geometry budget, recognizable sedan, modeled lights and interior. |
| Toyota Corolla Levin AE92 GT APEX | Nix_Dektara | 109,603 | 11,932,312 bytes | Detailed street coupe; substantial optimization required, 247 meshes. |

Files, SHA-256 hashes, original API metadata, source preview images and library
license documentation: `out/street-car-sourcing-20261028/`.
`download-report.json` contains source URLs, attribution, file measurements and
required glTF extensions. Counts are calculated from downloaded GLB index accessors.

Original sources:
- https://sketchfab.com/3d-models/762b90efd3dc413cb61c3e3c8097b8ac
- https://sketchfab.com/3d-models/06f0eba84e9745e9aec41a985ccac915
- https://sketchfab.com/3d-models/66dcbaf822ae47468655a604a83af703

All three source thumbnails were visually inspected. They show promising realistic
geometry, not stylized placeholder assets. These are creator previews, NOT renders
of the downloaded, simplified GLBs. Library copies have already been simplified
and meshopt-compressed; decoded local visual inspection is still required.

No cars have yet been added to the game catalogue, packaged as mods or installed
on the RP5. Next: decode meshopt, inspect actual geometry from multiple views,
check UVs/textures/wheel separation and scale, then adapt the Integra pilot through
the native importer. Validate paint/vinyls, damage, collision, wheel replacement,
performance, save round-trip and actual gameplay before installing. Repeat for
BMW and optimized Corolla; reject any model that fails visual acceptance.
