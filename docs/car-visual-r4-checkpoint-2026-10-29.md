# Checkpoint — autos r4 y validación, 29 de octubre de 2026

La revisión está aplicada en Linux. El ZIP de Android está generado y validado;
la instalación de esta revisión en la RP5 sigue pendiente. No es un cierre de
la verificación completa del port.

## Correcciones confirmadas

- Street Cars r4: Integra DB8, BMW E36 y Levin conservan el baúl/portón al cambiar
  el capó o el vinilo y seleccionar ningún alerón. El importador anterior había
  separado parte de esa carrocería como alerón desmontable. Ahora permanece en
  el chasis, junto con el alerón integral de fábrica.
- Ferrari r4: el carbono usa el panel delantero completo del modelo fuente,
  identificado por su conectividad, en lugar de un rectángulo interior.
  Se conservan superficie, bordes y emblema. Cambian cinco meshes por variante.
- Wave 2 permanece en r3, con las reparaciones previas de ruedas, capós, vinilos,
  faros y matrícula. No se ha hecho un remodelado artístico completo de sus fuentes.
- Linux ahora conecta gamepads SDL al sistema real de acciones: antes la lista
  de mandos estaba vacía. Incluye gatillos independientes, remapeo/zona muerta,
  órdenes de equipo, slots estables y neutralización al desconectar. El acceso
  directo a carrera prefiere un mando conectado. Sin prueba de mando físico aún.

Las calibraciones de física y el audio no se modificaron con las reparaciones
visuales. Los IDs de autos y las partidas se conservan.

## Evidencia

| Comprobación | Resultado |
| --- | --- |
| Regresión de carrocería con paquete anterior | 6 fallos esperados: 3 autos × garaje/carrera |
| Primeros tres autos corregidos | 377 comprobaciones aprobadas |
| Regresión del capó Ferrari anterior | 4 fallos esperados: panel completo y carbono × garaje/carrera |
| Ferrari corregida | 226 comprobaciones aprobadas |
| Pack completo extraído del ZIP | 1238 comprobaciones aprobadas |
| SHA256 de paquetes extraídos/instalados | Coinciden |
| Archivos de perfiles Linux antes/después | Los 6 son idénticos |
| Build completo / CTest final | Build correcto, 6/6 suites |
| Integración completa del port | 231 aprobadas, 1 falla: inventario estricto de 119 D8M pendientes |

Galería final: `out/validation/linux-current-20261029/final-car-gallery/index.html`.
384 capturas en Radeon RX 7800 XT: 11 autos agregados y un Focus de referencia,
ocho ángulos y cuatro configuraciones. No son una medición de FPS ni prueban
los 31 vinilos y todas las combinaciones de piezas. Los baselines de Xvfb usan
llvmpipe y están identificados por separado. Las capturas RGB se convierten sin
pérdida; no se interpreta el alpha del framebuffer como transparencia de ventana.

Las pruebas con mando virtual incluyen navegación real de menú/carrera, RT/LT,
dirección, pausa/reanudación, dos mandos y desconexión/reconexión. No se usa
--autopilot en la prueba de conducción. El oráculo negativo con el backend vacío
falla al tomar el jugador. No se afirma latencia o rendimiento de hardware físico.

## Entrega y respaldo

ZIP: `out/releases/OpenJuice-Car-Visual-Fixes-r4.zip`, 185023891 bytes.
SHA256: `b03ee745196ca6d6641dd060655cc208a79aba361a12be8784cd0f3aabbada06`.
Contiene Ferrari r4, Street Cars r4 y Wave 2 r3; requiere un APK con roster de
64 autos. No contiene saves. No se reinstaló el APK ni se editó dinero, autos o respect.

Instalación local: `~/.local/share/openjuice/mods`. El launcher habitual ejecuta
`out/linux/juiced` y usa esos paquetes. Versiones anteriores, scripts, logs,
recibos y hashes están en `out/validation/linux-current-20261029`, especialmente
`first-wave-r4`, `ferrari-r4`, `complete-car-visual-r4` y
`final-car-deployment-receipt.json`. Identidad de ejecutables:
`final-delivery-binaries-sha256.txt`.

La RP5 apareció brevemente por USB y se desconectó antes de iniciar respaldo o
importación. ADB volvió a detectar únicamente la iPlay, que no se utilizó.
Por lo tanto no hay una instalación Android r4 ni una prueba visual/manejo en RP5
que se puedan dar por aprobadas. No se hizo push ni se publicó un release nuevo.
El objetivo global conserva los pendientes de `verification-closure-2026-10-29.md`.
