# Reinicio de la adaptación de vinilos: seguir el sistema nativo

Estado: análisis del código y de los recursos locales completado para el recorrido
básico; conversión nueva, validación visual en Android, kits y daños pendientes.
Este documento sustituye el proceso de aprobación del pack v2/v3. Sus etiquetas
A/B no certifican la calidad de una nueva entrega. El pack instalado no se ha
modificado durante esta investigación.

## Lo que comprobamos

Inventario reproducible de `../game-data`, leyendo los DAT sin alterarlos:

- 54 bloques de definición: 52 coches con vinilos y dos entradas sin ellos (`-`,
  `none`). No son 54 coches con gráficos compatibles.
- Cada uno de los 52 coches contiene los mismos 31 nombres de estilo, en las
  variantes de garaje y carrera: **3.224 instancias de malla** leídas, sin errores
  de resolución de recursos.
- Los dibujos se resuelven a **31 texturas compartidas de `cartextures`**. Son
  motivos de **256×64**, no mapas cuadrados de una carrocería completa.
- Las **31 miniaturas de 256×64** son recursos `UI_DECAL_*` compartidos. La
  inspección de las imágenes confirma motivos aislados con transparencia, no
  capturas del coche o del taller. Algunas orientaciones difieren entre icono y
  textura: no asumir que copiar bytes siempre produce el icono correcto.
- La geometría y las UV se prepararon por coche **y por diseño**. En 96 de las
  104 combinaciones coche/variante las 31 mallas tienen geometrías diferentes;
  otras 4 tienen 30, 2 tienen 14 y 2 comparten una sola. Por tanto, tampoco es
  correcto sustituir todas las texturas sobre una supuesta malla universal.
- En el inventario inicial, 1.608 de los 1.612 pares garaje/carrera tenían bytes
  diferentes. Las mallas nativas tienen entre 58 y 624 vértices por recurso.
  Eso no mide por sí solo rendimiento, pero obliga a tratar ambas variantes.

Los hashes, rutas resueltas, posiciones, UV, materiales y cabeceras de textura
están en `out/vinyl-native-audit/retail-inventory.json`. Son observaciones de los
recursos locales, no una certificación del ejecutable comercial ni de todas las
combinaciones jugables.

## Recorrido del motor

1. `scripts/cardefs.txt` declara `CUSTOM DECAL <coche>_DECAL_<estilo>` por coche.
   `GameData`/`CarDefs` construyen la lista de piezas de categoría 8.
2. `CarModel::fitParts` selecciona la pieza guardada; `fillBody` la instala en el
   nodo `DECAL`. `CarModel::mesh` busca primero recursos del coche y después
   compartidos. La variante `_ui` corresponde al garaje.
3. Cada malla proporciona geometría y coordenadas UV explícitas. El motor no
   convierte automáticamente un atlas NFS ni adapta un dibujo arbitrario a la
   silueta de ventanas, pasos de rueda o paneles.
4. Los materiales nativos usan tanto `Coloured Spec ABlend` como `Standard Alpha
   Blend`: los primeros permiten color e iluminación especular; los segundos
   cubren motivos a color fijo. No todos los originales son recoloreables.
5. `DecalMenu::enter` deriva la miniatura quitando el prefijo del coche y
   anteponiendo `UI_` a `DECAL_...`. La lista, precio, bloqueo de prototipo y
   selector de color forman parte del comportamiento, no sólo de la imagen.
6. `CarModel::colourDecal` aplica el color a `Coloured Spec ABlend`. Las reglas
   del taller dependen de índices/estilos; no basta con cambiar el material de
   un importado para que el selector de color funcione correctamente.
7. Garaje y carrera registran `DECAL` en la deformación de daños. Usar una malla
   que carga no demuestra que se ajuste a todas las carrocerías y estados.
8. La partida conserva la selección de la categoría 8 como byte; varios caminos
   la interpretan como `int8_t`, con `127` para ninguno y `255` sin selección.

Fuentes: `src/Core/Data/GameData.cpp`, `src/Core/Cars/CarDefs.cpp`,
`src/Core/Cars/Car.cpp`, `src/Game/Cars/CarModel.cpp`,
`src/Game/Cars/CarView.cpp`, `src/Game/Race/TrackView/TrackViewLoad.cpp`,
`src/Game/Frontend/Garage/Customise/Styling.cpp`,
`src/Render/Materials/StandardMaterials.h`.

## Diferencias del importador anterior

| Aspecto | Juiced nativo | Pack v3 publicado |
| --- | --- | --- |
| Catálogo | Mismos 31 estilos en 52 coches | 107 opciones repartidas entre 19 modelos; Civic 10 |
| Arte | Motivo transparente compacto | Atlas NFS de múltiples zonas, con composición ligada al coche de origen |
| Colocación | Malla y UV preparadas por coche/estilo | Copia de superficies pintadas y proyección por normales/ejes |
| Ajuste | Contorno del diseño y carrocería tratados juntos | Escala por límites globales; no garantiza que texto o motivos eviten ventanas |
| Miniatura | Motivo claro, transparente, 256×64 | Captura de coche recortada, 128×64, con fondo y detalle reducido |
| Material | Color fijo o recoloreable según estilo | `Standard Alpha Blend` para todos, paletas horneadas |
| Prueba | Referencia visual y funcionamiento a reproducir | Carga, recursos y roundtrip; evaluación visual insuficiente |

`build.py::shell` usa normales de triángulos para decidir cinco zonas y los
extremos del coche para normalizarlas. `refine.py::balanced_sides` recorta/centra
arte parcial, pero no calcula el espacio real de las puertas bajo las ventanas;
los diseños de gran cobertura conservan el atlas de origen. Es la causa
estructural de los recortes, no un defecto que se resuelva subiendo resolución.
`thumbnails.py` reduce una captura con encuadre fijo: es otro proceso distinto
del icono nativo.

La aceptación previa confundió carga correcta con adaptación correcta. En los
dos `yourmom` del Civic, por ejemplo, se aprobó como contenido/legible un texto
con la primera línea cortada. Una hoja pequeña de tres vistas no debe ser la
única evidencia de aprobación.

## Catálogo y partidas: requisito previo al pack universal

El importador actual rechaza nuevas piezas al llegar a 125. Con 31 originales
admite 94 nuevas y deja margen para Custom Logo. Hacer universales las 107 de v3
exigiría 139 entradas contando originales y Custom Logo. Paginarlas en pantalla
no resuelve el byte guardado ni los casts con signo.

Antes de superar ese límite hay que diseñar una identidad estable de vinilo
separada del índice de pieza: recorrido de selección, render, compra y guardado,
con persistencia adicional compatible con las carreras existentes. Una tabla
auxiliar por coche/instancia es una opción a probar, no una solución implementada.
No modificar a ciegas el tamaño del registro binario de carrera. Tampoco barajar
índices existentes al eliminar diseños.

Se mantiene la instrucción del usuario: **una sola colección de saves, sin
separación por mods, sin bloquear partidas por un juicio de compatibilidad**.
El análisis no autoriza borrar ni reiniciar carreras. Los fallos reales de mods
se investigan como tales.

## Proceso nuevo obligatorio

1. **Referencia nativa.** Conservar el inventario y producir capturas comparables
   de estilos originales: motivo fino, texto/logo y gráfico multicolor. Registrar
   coche, pieza, pintura, cámara, variante y plataforma.
2. **Extraer el diseño, no trasladar el atlas completo.** Separar motivos laterales,
   logos de capó y decoraciones secundarias con sus máscaras. Preservar composición,
   proporción, texto, colores originales y atribución. Un diseño asimétrico debe
   mantener texto legible y orientación deliberada en ambos lados.
3. **Prototipo de colocación nativa.** Estudiar las UV y superficies de las mallas
   originales como plantillas por coche. Elegir o construir la superficie adecuada
   al motivo y ajustar las UV; no suponer que PROTO o cualquier estilo soporta
   todas las siluetas. Comprobar cobertura del arte y zonas seguras respecto de
   ventanas, ruedas y cambios de panel. Las métricas de cobertura ayudan, pero
   no sustituyen la revisión visual de letras y motivos.
4. **Piloto pequeño antes del lote.** Tres diseños exigentes (gráfico fino,
   texto/logo y multicolor) en Civic, Eclipse, Supra, Skyline, un hatchback y un
   coche de proporciones distintas. Comparar con originales a igual cámara,
   pintura e iluminación. Crear las variantes de garaje y carrera. Sólo después
   extender esos pilotos a los 52 coches originales.
5. **Miniatura nativa.** Crear el motivo aislado y legible en 256×64 con transparencia,
   márgenes y proporción preservada. No aplastar el motivo ni usar captura de coche.
   Comprobarla al tamaño real en el selector de Android, junto a los iconos originales.
6. **Funcionamiento nativo.** Verificar catálogo común, selección, compra, precio,
   color cuando corresponda, cambio de vinilo, None y Custom Logo. Resolver el límite
   del catálogo antes de prometer todos los diseños universales.
7. **Validación por coche, no por pack.** Carga de las dos variantes, capturas grandes
   de ambos lados y frente/parte superior; colores claros/oscuros; kits pertinentes;
   carrera, cámara cercana/lejana y daño. Cargar/guardar una carrera de prueba sin
   alterar carreras reales. Revisar transparencias, clipping y parpadeo en Android.
8. **Puerta de calidad.** Texto completo; motivo principal sin cortes accidentales;
   escala coherente; sin estiramientos, zonas flotantes o fragmentos aislados; icono
   reconocible. Un B no puede esconder un defecto funcional de mapeado. Registrar
   resultados por diseño × coche × variante y fallos pendientes.
9. **Entrega.** Sólo después del piloto aprobado, ampliar el catálogo. No publicar
   ni instalar automáticamente otro lote a partir de una prueba de carga exitosa.

Para coches añadidos por mods (por ejemplo Ferrari) hace falta una plantilla y
validación propias. Que los 52 originales compartan catálogo no demuestra que
un modelo externo tenga las superficies necesarias. Los diseños de carrocería
completa tampoco se vuelven universales sólo extrayendo una franja.

## Reproducción y próximos puntos no resueltos

```sh
python3 tools/vinyl_pack/audit_retail.py --game ../game-data \
  --package out/vinyl-research/v3-curated --output out/vinyl-native-audit
```

El lector usa NumPy y el extractor local de DAT. Guarda hashes y metadatos; no
incluye recursos comerciales en Git. Imágenes locales inspeccionadas:
`out/vinyl-native-audit/retail-thumbnails.png` y `native-texture-vs-icon.png`.

Quedan pendientes la elección/calibración de plantillas por motivo, la prueba
visual del piloto, la compatibilidad de kits/daños y la extensión del catálogo.
Este checkpoint contiene el diagnóstico y el proceso, **no un pack corregido**.

Resumen verificable versionado: `vinyls-native-audit-summary-2026-10-26.json`,
con hash del inventario local completo, hash de cardefs, recuentos y comparación.
Se verificó además que los 52 coches tienen el mismo orden de los 31 estilos.
