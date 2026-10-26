# Revisión completa del pack contra el criterio nativo

Resultado: **241 revisados, 0 admitidos, 241 excluidos de la próxima selección**.
La cobertura incluye las 107 opciones de la entrega publicada y las 134 que ya
se habían retirado. Esto no es una nueva calificación estética del arte original:
es una evaluación de las conversiones existentes contra el contrato solicitado.

Cada opción falla dos prerrequisitos verificables en sus recursos actuales:

- Sólo tiene registro y mallas de garaje/carrera para un coche, en lugar de los
  52 coches originales con catálogo compartido.
- Su miniatura mide 128×64. El pipeline que produjo estas miniaturas utiliza
  capturas de coche, en vez del motivo transparente de 256×64 del sistema nativo.

Además, ninguna cuenta con la nueva aceptación visual y funcional por coche.
Una nota A/B previa no sustituye esa prueba. No se afirma haber reinspeccionado
manualmente 723 vistas en esta pasada: los prerrequisitos anteriores bastan para
excluir todas las conversiones de una entrega que prometa el funcionamiento
nativo. No se infiere que todos los dibujos fuente sean inutilizables.

`tools/vinyl_pack/native_review.json` registra la decisión individual, ID, nombre,
pack de origen, coche, registro, variantes disponibles, tamaño de miniatura,
nota anterior, pertenencia a la entrega de 107 y motivos de exclusión. Incluye
hashes del catálogo, registro e inventario nativo usados. `accepted_ids` está vacío.

`curate.py` ahora exige la revisión nativa correspondiente al catálogo/registro
fuente, además de la revisión visual. Con la evidencia actual rechaza generar
el mod antes de crear el directorio de salida. Se comprobó también que marcar
artificialmente todas las notas antiguas como A no permite incluirlas.

No se generó un ZIP vacío ni se cambiaron los assets instalados en Android,
el paquete publicado, los vinilos equipados o las partidas. El pack anterior
se conserva como referencia; no representa la próxima selección aprobada.

La readaptación debe seguir `vinyls-native-first-2026-10-26.md`: extraer motivos,
preparar colocación por coche, icono nativo y validar el piloto antes de admitir
un diseño. El filtro por sí solo no repara los recursos ni crea compatibilidad.

Reproducir:

```sh
python3 tools/vinyl_pack/audit_native_gate.py \
  --package out/vinyl-research/v2-reviewed \
  --retail out/vinyl-native-audit/retail-inventory.json \
  --published-catalogue out/vinyl-research/v3-curated/catalogue.json \
  --output tools/vinyl_pack/native_review.json
```

El inventario nativo se genera con `audit_retail.py`, documentado en el checkpoint
anterior. Las herramientas leen recursos locales y no modifican las fuentes.
