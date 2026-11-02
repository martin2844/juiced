# Checkpoint — correcciones de autos r5, 29 de octubre de 2026

La entrega combinada r5 está instalada en Linux. Contiene Ferrari r4, Street Cars
r4 y Wave 2 r4. Sustituye la entrega combinada r4 del checkpoint anterior.
La RP5 no aparece en ADB: esta revisión aún no está instalada ni probada allí.

## Cambio adicional en el Jetta

La galería anterior mostraba faros delanteros grises y dos picos negros junto a
la luneta. Se recuperaron los reflectores/marcos originales del modelo fuente,
que estaban desplazados 1,68 m por encima de sus lentes. Se conservan sus formas;
las lentes exteriores son transparentes y las superficies de luz alta se activan
por separado. Los materiales se adaptan al motor de Juiced.

Los picos eran del revestimiento interior del baúl, no del marco de la luneta.
Se identificaron mediante capturas que colorean cada material y luego cada pieza
por separado. Ocultar el marco no resolvió el problema; esa prueba se descartó.
El marco original permanece. Se ajustaron 28 vértices del revestimiento para
dejarlos dentro de la superficie real de carrocería/vidrios. Las otras 217 piezas
compartidas entre los GLB anterior y nuevo conservan su geometría.

`tools/street-cars-wave2/repair_jetta_lamps.py` guarda un blend/GLB separado y un
recibo con hashes de entradas y posiciones anteriores/nuevas. Los originales no
se sobrescriben. Los 31 meshes de vinilos del Jetta son byte por byte idénticos
en ambas variantes. Las recetas de física y los assets de los otros seis Wave 2
son idénticos a la revisión anterior. No cambia el roster ni sus IDs.

## Validación y entrega

- Galería final dentro del motor, Radeon RX 7800 XT: 32 imágenes, ocho ángulos
  de stock, carbono, vinilo 8 y kit máximo con carbono/vinilo 8. Se revisó la
  hoja completa: faros presentes, picos ausentes y carrocería conservada.
- ZIP extraído independientemente: 1.238 comprobaciones del roster completo,
  377 de Street Cars y 226 de Ferrari; cero fallos.
- Build completo de Linux y seis suites CTest aprobados. La prueba reforzada
  de pink slips aprueba 54 comprobaciones de setup, confirmación/autosave y
  transferencia de autos al ganar/perder. Usa un host de pruebas; no certifica
  una carrera física completa ni una escritura de autosave en Android.
- Los archivos de perfiles Linux bajo ModSaves y saves se comparan antes/después
  de instalar. Son idénticos. No se editaron dinero, autos ni respect.

ZIP: `out/releases/OpenJuice-Car-Visual-Fixes-r5.zip`, 186577905 bytes.
SHA256: `3537ef0d6454b7cdca73d09d4f4d1d92f011cba2d65f2c8bf91df20d280e1373`.
No contiene saves ni APK. Requiere el ejecutable existente con soporte de 64
entradas en el roster. El launcher habitual de Linux usa los paquetes instalados
en `~/.local/share/openjuice/mods`.

Evidencia: `out/validation/linux-current-20261029/jetta-lamps`, especialmente
`final-gallery/index.html`, `final-native-check.log`, `gltf-geometry-diff-v3.json`,
`candidate-v3/lamp-restoration.json`, `adoption-receipt.json` y
`deployment-receipt.json`. El ZIP y las verificaciones independientes están en
`complete-car-visual-r5`. Las versiones anteriores se conservan para rollback.

Los probes de colores y el candidato inicial con selección incorrecta de lentes
están identificados como descartados y no se instalaron. La galería de r4 sigue
sirviendo para los otros diez autos agregados; la de este checkpoint reemplaza
sus vistas del Jetta. Estas pruebas no certifican FPS Android, todas las
permutaciones de piezas/vinilos ni acabado artístico idéntico al juego original.
Tampoco cierran el port completo: permanecen los pendientes del registro global,
incluido el fallo estricto del inventario de 119 D8M.

## Verificación adicional de vinilos y Android

La galería `all-vinyl-gallery/index.html` cubre ambos lados de los 31 vinilos
originales en los once autos agregados y un Focus de referencia: 774 capturas
Radeon, 49 hojas de contacto revisadas visualmente. Ferrari incluye sus tres
slots adicionales; el receptor vacío de logos personalizados es intencional.
No se encontraron nuevos fallos de proyección lateral en esas vistas con kit
stock. El recibo `all-vinyl-gallery/visual-review.json` identifica las imágenes
y el ejecutable. No equivale a revisar todas las combinaciones de piezas/daños.

El ZIP r5 se importó mediante el selector de archivos normal en el emulador
Android: 64 autos validados y hashes de los tres paquetes comprobados. Una copia
de prueba del save Linux conserva sus bytes tras actualizar/importar; no se usó
el save de Ronnie. La RP5 sigue pendiente. El APK y las correcciones del renderer
y almacenamiento están documentados en
`android-render-storage-checkpoint-2026-10-29.md`.
