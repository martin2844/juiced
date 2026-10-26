# Revisión visual individual de vinilos v2

Se revisaron las 241 imágenes de la galería, cada una con tres vistas del coche
(723 vistas). La revisión es visual y manual; no se dedujeron las notas del
nombre del fichero ni del resultado de las pruebas de carga. Se montaron hojas
numeradas para recorrer todas las opciones y se volvieron a ampliar los candidatos
A desde las capturas originales. Esa segunda revisión degradó dos variantes de
franjas del 300ZX a C por bloques visibles en las aletas.

## Criterio y resultado

| Tier | Opciones | Criterio | Próximo paso recomendado |
| --- | ---: | --- | --- |
| A | 8 | Casi nativo: escala, composición y colocación limpias | Conservar |
| B | 99 | Se nota la adaptación, pero es coherente y aceptable | Conservar |
| C | 115 | Recortes, deformación, composición o legibilidad deficientes | Retirar |
| D | 19 | Fragmentación grave, texto destruido o resultado casi vacío | Retirar |

La selección propuesta conserva **107** y retira **134**. No se borró ni se
desactivó ningún recurso y el ZIP v2 permanece intacto. Las pruebas técnicas
anteriores no equivalían a aprobación de calidad visual: esta revisión aplica
el umbral más estricto solicitado por el usuario.

## Evidencia y uso

- `tools/vinyl_pack/visual_tiers_v2.json`: dictamen por ID estable, coche,
  diseño, pack, tier y motivo individual. Es la fuente versionada de decisiones.
- `tools/vinyl_pack/tier_gallery.py`: genera la galería filtrable y las listas.
  Verifica cobertura exacta del catálogo, IDs únicos y disponibilidad de imágenes.
- `out/releases/underground-vinyls-v2/gallery/index.html`: la galería existente
  ahora muestra tier, motivo y filtros por A/B/C/D, conservar y retirar.
- `out/releases/underground-vinyls-v2/tier-review/ratings.csv`: tabla completa.
- `out/releases/underground-vinyls-v2/tier-review/keep-ids.txt`: 107 IDs A/B.
- `out/releases/underground-vinyls-v2/tier-review/remove-ids.txt`: 134 IDs C/D.
- `out/releases/underground-vinyls-v2/tier-review/image-sha256.json`: hashes de
  las imágenes exactas revisadas, para detectar cambios posteriores.

Reproducir el informe, sin modificar assets ni saves:

```sh
python3 tools/vinyl_pack/tier_gallery.py out/releases/underground-vinyls-v2
```

El juicio corresponde al aspecto actual de las capturas, con carrocería de
fábrica. No certifica otras pinturas, kits, daños, vistas no mostradas o calidad
en movimiento. Los diseños poco legibles se calificaron tal como aparecen;
otra pintura podría justificar reevaluarlos, pero no se los conservó basándose
en una mejora hipotética. El tier evalúa la conversión visible, no la calidad del
trabajo original de los autores. Aceptar un diseño B tampoco significa que sea
fiel al coche de cine que lo inspiró.
