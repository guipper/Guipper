# PAINT

PAINT es el editor integrado de dibujo y animación cuadro a cuadro de Guipper.
Agregá una caja PAINT con `b`, seleccionala y abrí **PAINT** desde el encabezado
del inspector. El botón `?` del panel muestra la lista completa de controles.

## Dibujo

Las herramientas disponibles son pincel (`B`), goma (`E`), línea (`L`),
rectángulo (`R`), elipse (`O`), pluma rellena (`P`) y relleno de región (`G`).
Hacé clic en la etiqueta `TAM` a la izquierda del deslizador para elegir
tamaño, opacidad, dureza o estabilización; el menú muestra y permite ajustar
cada propiedad. Con la herramienta de relleno el control pasa a ser
tolerancia. `[` y `]` cambian rápidamente el tamaño del pincel.

El color principal abre un selector con entrada hexadecimal. El botón `+`
guarda el color en la paleta y el clic derecho elimina una muestra. Si el
sistema entrega presión de lápiz o touch, PAINT la aplica al ancho del trazo.

## Selección

`S` activa la selección libre y `M` la rectangular. La selección recorta los
trazos en el límite sin deformar las partes elegidas. `Shift` suma otra zona y
`Alt` resta. Luego podés:

- arrastrar para mover;
- usar las esquinas para escalar y la manija superior para rotar;
- usar las flechas para mover un píxel, o diez con `Shift`;
- voltear horizontalmente con `H` o verticalmente con `Shift+H`;
- duplicar con `D`, borrar con `Del`, confirmar con `Enter` o cancelar con
  `Esc`;
- copiar, cortar y pegar con `Ctrl/Cmd+C`, `Ctrl/Cmd+X` y `Ctrl/Cmd+V`;
- seleccionar toda la capa activa con `Ctrl/Cmd+A`.

Todas las operaciones de selección, incluidos movimiento, transformación,
corte, pegado y borrado, participan de `Ctrl/Cmd+Z` y `Ctrl/Cmd+Shift+Z`.

## Capas y línea de tiempo

Las filas representan capas y las columnas cuadros. Un clic elige ambos; se
pueden reordenar arrastrando sus encabezados. `N` crea un cuadro y `D` duplica
el actual cuando no hay una selección. `,` y `.` recorren cuadros, mientras
`<` y `>` recorren capas. `-` y `+` ajustan la duración del cuadro.

Cada capa tiene visibilidad, opacidad, bloqueo y modo de mezcla. El indicador
`N/M/S/+` alterna Normal, Multiplicar, Trama y Sumar. `Ctrl/Cmd+J` duplica la
capa activa. `Ctrl/Cmd+E` la combina hacia abajo cuando ambas capas son
normales, totalmente opacas, visibles y desbloqueadas; esa restricción evita
una conversión destructiva inesperada. La opción `BG` comparte una capa de
fondo entre todos los cuadros.

La barra de transporte controla reproducción, FPS, loop, ping-pong, dirección
y papel cebolla. La duración individual de cada cuadro también se respeta al
reproducir y exportar.

## Exportación

- `Ctrl/Cmd+Shift+P`: cuadro actual en PNG.
- `Ctrl/Cmd+Alt+P`: todos los cuadros como secuencia PNG numerada.
- `Ctrl/Cmd+Shift+G`: GIF animado usando los FPS y la duración de cada cuadro.

PNG conserva el canal alfa completo. GIF usa transparencia binaria por las
limitaciones del formato. Si el documento tiene fondo, se compone en la imagen
exportada igual que en la salida de la caja.

## Compatibilidad

Los documentos PAINT anteriores continúan cargando. Los campos que no existían
adoptan valores conservadores: dureza completa para trazos antiguos, capas
desbloqueadas y modo de mezcla Normal. La paleta se guarda en
`data/paint_palette.xml`.

La entrada de textura funciona únicamente como referencia para calcar dentro
del editor; nunca forma parte de la salida de la caja.
