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

`Y` cambia la simetría de dibujo: apagada, espejada en el eje vertical, en el
horizontal o en los dos. Los ejes activos se ven como guías punteadas y cada
trazo confirma sus espejos como trazos reales, así que después se pueden
borrar, seleccionar y mover por separado. Un solo `Ctrl/Cmd+Z` deshace el
trazo con sus espejos.

El balde guarda la **región** que el relleno cubrió, no el clic que lo hizo.
Por eso un relleno se puede seleccionar, mover y transformar como cualquier
otra marca, no cambia si después se edita algo debajo, y no cuesta nada
reconstruir el cuadro. Los documentos viejos siguen mostrando sus rellenos
como antes.

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

La línea de tiempo admite selección por bloques: un clic selecciona una celda,
`Shift`+clic extiende un rango rectangular desde la celda ancla y `Ctrl`+clic
(Windows/Linux) o `Cmd`+clic (macOS) alterna celdas individuales. Arrastra una
selección para mover el bloque; solo se aceptan destinos vacíos fuera del
bloque de origen. `Ctrl/Cmd+C` copia, `Ctrl/Cmd+X` corta y `Ctrl/Cmd+V` pega
el bloque desde la celda activa. `D` duplica el bloque y `Del`/`Backspace` lo
borra. Cada operación se deshace como una sola acción.

Cada capa tiene visibilidad, opacidad, bloqueo y modo de mezcla. El indicador
`N/M/S/+` alterna Normal, Multiplicar, Trama y Sumar. `Ctrl/Cmd+J` duplica la
capa activa. `Ctrl/Cmd+E` la combina hacia abajo cuando ambas capas son
normales, totalmente opacas, visibles y desbloqueadas; esa restricción evita
una conversión destructiva inesperada. La opción `BG` comparte una capa de
fondo entre todos los cuadros.

La barra de transporte controla reproducción, FPS, loop, ping-pong, dirección,
papel cebolla y simetría. La duración individual de cada cuadro también se
respeta al reproducir y exportar.

El control `ONION` muestra los dos lados del rango. Un clic los mueve juntos;
`Shift`+clic cambia solo los cuadros anteriores y `Alt`+clic solo los
siguientes.

## Exportación

- `Ctrl/Cmd+Shift+P`: cuadro actual en PNG.
- `Ctrl/Cmd+Alt+P`: todos los cuadros como secuencia PNG numerada.
- `Ctrl/Cmd+Shift+G`: GIF animado usando los FPS y la duración de cada cuadro.
- `Ctrl/Cmd+Alt+G`: hoja de sprites con todos los cuadros en una grilla.

El ícono de hoja en el encabezado del panel abre las opciones: un
multiplicador de resolución (0.5x a 4x) y si se exporta todo el documento o
solo el rango `IN`/`OUT` del transporte. Como los trazos son vectoriales,
exportar más grande vuelve a rasterizar la geometría en vez de estirar
píxeles.

PNG conserva el canal alfa completo. GIF usa transparencia binaria por las
limitaciones del formato: el color clave se elige entre los que el cuadro no
usa, así que un dibujo en magenta ya no sale con agujeros. Si el documento
tiene fondo, se compone en la imagen exportada igual que en la salida de la
caja.

## Compatibilidad

Los documentos PAINT anteriores continúan cargando. Los campos que no existían
adoptan valores conservadores: dureza completa para trazos antiguos, capas
desbloqueadas, modo de mezcla Normal y simetría apagada. Los
rellenos guardados con el formato viejo (semilla y tolerancia) se siguen
dibujando como siempre; los nuevos se guardan como región. La paleta se guarda
en `data/paint_palette.xml`.

La entrada de textura funciona únicamente como referencia para calcar dentro
del editor; nunca forma parte de la salida de la caja.
