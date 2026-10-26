# Colección depurada A/B — 2026-10-26

Aplicada la selección de `visual_tiers_v2.json`: 107 opciones (8 A y 99 B).
Las 134 opciones C/D se excluyen tanto del catálogo como de sus recursos:
mallas de garaje y carrera, materiales, texturas y miniaturas. La edición
contiene exactamente 536 archivos de assets: cinco por opción y el registro.
Los IDs y el orden relativo de los diseños conservados no cambian.

## Entrega local

- `out/releases/underground-vinyls-v3-curated/OpenJuice-Underground-Vinyls-v3-curated.zip`
- `out/releases/underground-vinyls-v3-curated/gallery/index.html`
- `out/releases/underground-vinyls-v3-curated/native-validation.log`

ZIP: 19,728,919 bytes, frente a 54,387,675 bytes de v2. Conserva créditos,
procedencia, evaluación completa y listas de IDs incluidos/excluidos.
La galería nueva contiene únicamente los 107 diseños incluidos. La galería
anterior y el ZIP v2 siguen disponibles para comparación y recuperación.

## Verificación

- Correspondencia exacta entre evaluación, catálogo, registro y recursos.
- Prueba nativa: `Validated 107 vinyls in garage/race plus save roundtrip`.
- Integridad del ZIP comprobada por el empaquetador.
- El paquete v2 permanece sin modificaciones.

Reproducción:

```sh
python3 tools/vinyl_pack/curate.py out/vinyl-research/v2-reviewed out/vinyl-research/v3-curated
DISPLAY=:0 out/android-host/android_vinyl_check ../game-data out/vinyl-research/v3-curated/assets
python3 tools/vinyl_pack/package.py --package out/vinyl-research/v3-curated \
  --sources out/vinyl-research --captures out/vinyl-research/v2-reviewed-shots \
  --inventory out/vinyl-research/decoded-v2/catalogue.json \
  --version v3-curated --output out/releases/underground-vinyls-v3-curated
```

Los destinos deben ser nuevos. No se instaló en NixOS/Retroid ni se tocaron
partidas. La edición sustituye a la colección completa: no activar ambas a la
vez. La eliminación cambia índices de vinilos y la variante de catálogo; no
copiar saves de la colección completa a esta variante sin remapear los índices.
Se mantienen el requisito de extensión nativa y el alcance visual de carrocerías
de fábrica; esta prueba de host no certifica rendimiento Android.
