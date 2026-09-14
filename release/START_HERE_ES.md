# Tu primera visual en cinco minutos

1. Abrí Guipper. La distribución de inicio carga la composición de anillos.
2. Seleccioná **Rings** y cambiá `density`. Los controles rojo, verde y azul cambian el color.
3. Hacé doble clic en el nodo para elegirlo como salida activa.
4. Guardá una copia con un nombre propio. Cerrala y volvé a abrirla para comprobar dónde quedó.
5. Abrí AUDIO y elegí una entrada de audio. Ajustá `audio_amount` para controlar cuánto cambia el brillo con los graves. Primero comprobá la entrada con el medidor; no requiere activar el micrófono para funcionar como visual generativa.

## Los tres ejemplos

- `savefiles/examples/01-generative.xml`: anillos animados, densidad y color.
- `savefiles/examples/02-audio.xml`: la misma base para practicar la selección y calibración de audio. Sin señal se sigue viendo la visual.
- `savefiles/examples/03-mix.xml`: mezcla los anillos con una imagen de prueba incluida. Seleccioná **Mix** y ajustá `amount`. Arrastrá una imagen o video propio al canvas y conectalo a una entrada para probar tus medios.

F10 abre versión, actualizaciones y exportación de diagnóstico. En una compilación
sin canal firmado, las actualizaciones aparecen como no disponibles.

Si una recuperación está disponible al iniciar, Guipper muestra F9 para abrirla
y F8 para descartarla. Antes de abrirla guarda el estado actual en
`savefiles/before-recovery.xml`. Guardá después la recuperación con un nombre nuevo.

Para reportar problemas: exportá el diagnóstico desde F10 y abrí un issue en
https://github.com/guipper/Guipper/issues. Incluí los pasos y lo que esperabas ver.
Los archivos no se envían automáticamente.
