# Partículas Kinect

Crear con **Shift+K** en NODES. Requiere Kinect v2 con el controlador ya utilizado por Guipper. No hace falta agregar una caja KINECT2: comparte la captura de profundidad con las otras cajas y con Público. No usa RGB ni detección de manos, por lo que no depende de iluminación visible.

La caja entrega una textura normal para conectar a shaders o enviar a OUTPUT/FINAL. El estado sobre los parámetros informa si espera profundidad, no hay muestras en rango o cuántas partículas y cuadros de historia utiliza. Pausar detiene y libera su participación en la captura; eliminar también, aunque undo conserve el objeto. Reanudar empieza una simulación nueva.

## Controles

| Control | Uso |
| --- | --- |
| density | Densidad de la silueta y tasa de emisión. Cero deja extinguir las partículas existentes. |
| point size | Tamaño de las partículas en píxeles de referencia, corregido por distancia. |
| life s | Duración de las partículas; variación determinista de ±20 %. |
| turbulence | Movimiento espacial en dos frecuencias. |
| rise m/s | Ascenso o descenso; también desplaza ligeramente hacia la cámara. |
| spread | Expansión horizontal y vertical. |
| delay s | Retardo máximo de las franjas, limitado a la historia disponible. Cero muestra la silueta actual. |
| time stripes | Cantidad de franjas que consultan diferentes momentos de la profundidad. |
| silhouette | Opacidad de la silueta. Cero muestra solo las partículas desprendidas. |
| near m / far m | Recorte de profundidad. Por defecto 3–4,5 m para público. Far se limita internamente a un mínimo de near + 1 cm. |
| zoom | Encuadre. |
| color cycle / brightness | Tono de la estela e intensidad; la silueta conserva su blanco. |
| mirror | Espejo horizontal, activado inicialmente. |
| additive | Suma luminosa de partículas superpuestas. |

Para probar cerca del escritorio, bajar **near m** a 0,7. Para un contorno más reconocible, bajar turbulence, spread y delay; subir silhouette. Para una disolución más marcada, aumentar life y turbulence y reducir silhouette. Todos los controles numéricos usan las automatizaciones habituales.

## Relación con Kinect_SARV.toe

Referencia suministrada por el usuario: SHA256 `226f4a2b11064f54d1fdc05cd609ad9e7abc35fa6c3d2b915103737d50cc037d`. La red fue extraída con toeexpand oficial, sin ejecutar scripts del proyecto. Se identificaron Kinect depthpointcloud → historia/Time Machine por franjas → instancias, más partículas con realimentación, vida, expansión, dos campos de ruido y color por edad. El componente de vida del original acredita a Supermarket Salad; el selector de paleta a Colin Chibois.

Esta es una implementación propia de esas dos capas, no un ejecutor de TouchDesigner ni una reproducción idéntica verificada. Usa simulación CPU acotada y dibujo de sprites esféricos en GPU. Los cubos/esferas geométricos, el ruido exacto de TouchDesigner, su selector de paletas y la equivalencia visual cuadro a cuadro quedan fuera de esta primera versión. No copia código de esos componentes.

La captura y decodificación permanecen en el servicio compartido. La simulación corre en update; su coste se acota a 128.000 partículas y una rejilla de profundidad de paso mínimo 1. Conserva como máximo 32 snapshots de profundidad (~28 MB), sin copiarlos por consumidor. Sin profundidad nueva durante 500 ms deja de emitir y vacía la historia; las partículas existentes terminan su vida. dt se limita a 100 ms para evitar ráfagas después de un bloqueo. La simulación y sus cuadros no se guardan: el codec normal conserva solo parámetros y vínculos. El token persistente es `kinectParticles`, con un tipo añadido al final de la enumeración.

## Verificación

- `make -C tests kinect_particles_tests && ./tests/kinect_particles_tests`: recorte, NaN, orientación, historia, vencimiento, límites, cambios de formato y emisión a 30/60 Hz.
- `python3 tests/kinect_particles_graphics_test.py`: shaders reales, figura de profundidad sintética, salida a otro shader, cambio de resolución, XML, duplicación y delete/undo/redo; genera PNG en `/tmp` y usa una copia aislada de datos.
- Agregar `--live` prueba 20 segundos de Kinect real y registra el coste de update/render de la caja. No equivale a medir una composición de show completa.
- Linux es la plataforma de validación local. Qbs y Visual Studio incluyen las fuentes; no se declara compilación ni prueba real en Windows/macOS.
- La comparación con el render original de TouchDesigner y el ajuste con público en el espacio real siguen pendientes.

### Resultado local — 2026-10-21

Release Linux compiló. Pasaron los tests del motor, atajos y el harness gráfico (incluyendo la orientación vertical y la restauración del scissor del inspector). Se revisaron la imagen de profundidad sintética y los 16 controles del inspector. La prueba real de 20 s con Kinect v2 y RTX 4050 produjo hasta 14.260 partículas: 1.161 actualizaciones, coste CPU de update + envío de dibujo medio 1,156 ms y máximo 4,117 ms. La medición no incluye una espera de GPU ni una composición completa, y no representa la tasa del sensor. La prueba real amplió el recorte a 0,7–4,5 m para el entorno local. El ensayo con público exclusivamente a 3–4,5 m queda pendiente.

Los artefactos y logs de esta ejecución quedaron en `/tmp/guipper-particles-8_u_de1o`; no se añadieron capturas de la habitación al repositorio. Las composiciones y respaldos existentes se mantuvieron fuera de los cambios.

### Recursos internos y perfiles existentes

El shader de partículas se carga por ruta absoluta desde `AppPaths::bundle`, junto al ejecutable. No depende de que el perfil personal tenga una copia: los perfiles migrados de un checkout anterior no incorporan automáticamente archivos nuevos del proyecto. El harness gráfico cubre expresamente un directorio de datos personal vacío de shaders. Las composiciones y los shaders personales no se copian ni se sobrescriben para resolver esta dependencia.

### Ajuste a la referencia visual

La imagen de TouchDesigner aportada después de la primera versión muestra una silueta blanca densa y material rojo desprendido. La capa fuente ahora usa blanco y cobertura acorde al espaciado de la rejilla, con partículas que pasan de blanco a rojo por edad. `color cycle` gira el tono de la estela. Nuevas cajas parten con `density=1` y `silhouette=1`; las composiciones existentes conservan sus valores. La deformación por franjas continúa muestreando historia de profundidad, sin pintar franjas negras sobre una persona inmóvil. Este ajuste se verificó con la figura sintética móvil; la comparación del movimiento con TouchDesigner sigue pendiente. La medición de rendimiento anterior usaba density=0.55.

### Relieve y movimiento coherente

La siguiente iteración elimina el blanco plano de la superficie: estima normales con muestras vecinas de profundidad y rechaza saltos mayores de 12 cm para no sombrear el contorno usando el fondo. Los puntos fuente vuelven a ser finos, sin inflarlos para tapar huecos. La densidad máxima conserva los 512×424 puntos nativos. La geometría se recalcula al llegar profundidad nueva o cambiar sus controles, no en cada actualización de partículas.

Las partículas ahora comparten un campo espacial de dos frecuencias; la semilla individual afecta la duración pero ya no cambia la fuerza aplicada a cada una. Esto conserva la coherencia local de las estelas. El límite sube a 128.000 y la emisión máxima a 64.000/s. Sigue siendo simulación CPU y no reproduce todavía los Noise TOP ni el volumen de partículas del proyecto original. A 3–4,5 m, el detalle facial está limitado por la resolución de profundidad; la iluminación no recupera detalles que el sensor no midió.

Validación 2026-10-22: motor y harness gráfico pasan; prueba real de 20 s alcanzó 103.434 partículas, coste CPU de actualización/envío de dibujo medio 6,662 ms y máximo 17,135 ms. La escena probada incluye la habitación completa con recorte 0,7–4,5 m, no una persona aislada. Para comparar la figura con el original hay que recortar el fondo con near/far. La estela usa granos finos con transparencia suave y tamaño limitado en pantalla, evitando discos grandes al acercarse al sensor. La medición precede a este último cambio de tamaño/transparencia en shader.


## Definición de la superficie y el rostro

`surface detail` añade una superficie de puntos nativos del último cuadro de profundidad, sin retraso por franjas y sin depender de `density`. Valor inicial 0,85; cero recupera la silueta anterior. Al aumentarlo reduce la silueta retrasada para evitar que tape los rasgos actuales. Las partículas desprendidas conservan su vida, color y movimiento.

Los puntos de detalle tienen tamaño basado en la proyección del sensor y la resolución de salida; no se agrandan con `point size`. La iluminación usa un gradiente local con rechazo de agujeros/discontinuidades y ponderación de profundidad para reducir ruido. No interpola geometría ni inventa detalle facial. Es una mejora de toda la superficie, no un detector de caras; sigue funcionando solo con profundidad y sin luz RGB.

Para probar: `surface detail=1`, brillo alrededor de 1 y ajustar near/far al sujeto. Puede acercarse el sujeto para que su cara ocupe más muestras; zoom solo amplía lo captado. A 3–4,5 m la profundidad nativa de 512×424 limita mucho ojos, boca y nariz. La nube puede seguir tapando partes si se cruza por delante: reducir life/turbulence/spread para comparar. La semejanza facial requiere validación real; la simulación no la demuestra.

El nuevo parámetro se agrega al final para conservar índices guardados. El historial, emisión y conteo de partículas no crecen; hay una capa adicional de hasta un punto por píxel de profundidad. Mayor detalle añade trabajo de CPU y vértices, por lo que se debe medir en la composición real.

Validación local: Release, cálculo, render con superficie sola, orientación, conexión a shader, resize, persistencia y undo/redo aprobados. El patrón sintético no valida definición de un rostro real. El cálculo CPU de 30 superficies completas de 512×424 promedió 3,3 ms sin detalle y 9,9 ms con detalle, excluyendo GPU/captura; cualquier valor mayor que cero activa esa capa completa. Para rostros, comenzar con additive desactivado. Resultados: [particles-detail-linux-2026-10-23.json](../tests/results/particles-detail-linux-2026-10-23.json).


## Superficie continua y nube independiente

La capa `surface detail` ahora aplica su intensidad una sola vez como luminancia, con cobertura opaca de profundidad. Antes los puntos superpuestos acumulaban alfa y producían una trama visible; una superficie plana debe quedar uniforme. La exposición se comprime suavemente para conservar diferencias de luz incluso con brightness alto.

Controles agregados al final: **surface contrast** (0–2, inicial 1) ajusta las pendientes de luz sin cambiar la geometría; **plume opacity** (0–1, inicial 0,6) controla exclusivamente las partículas desprendidas. La nube comprueba profundidad contra el cuerpo, pero ya no escribe profundidad: sus bordes transparentes no bloquean otros granos. `additive` se aplica a la nube, manteniendo la superficie independiente.

Punto de partida para un rostro legible: surface detail 0,85–1, surface contrast 1–1,3, plume opacity 0,25–0,6 y point size 2–4. `point size=16` sigue generando granos grandes; no aumenta la resolución facial. El cambio conserva posiciones guardadas de parámetros y no modifica composiciones existentes. El brillo de la superficie cambia deliberadamente para evitar saturación.

Validación de render separado: Release y prueba gráfica aprobados; superficie plana con variación máxima de 2 niveles de 8 bits y margen de brillo con brightness=2,5; persistencia de los nuevos controles verificada. Revisadas capturas sintéticas con zoom=2,21 y point size=16. Falta comparar el rostro real y FPS de la composición. Resultados: [particles-surface-render-linux-2026-10-23.json](../tests/results/particles-surface-render-linux-2026-10-23.json).
