# Transiciones entre composiciones: MilkDrop, AVS y Guipper

Investigación de código, 2026-09-17. Guipper analizado: `0e6593fb906dfaf6d6cb2bd07e537b8db3a2f995`.

## Conclusión

Para mejorar las transiciones de Guipper hay dos trabajos diferentes: conservar la composición saliente y preparar la entrante; después, mejorar cómo se mezclan. La primera parte es indispensable para cambiar archivos de composición completos.

La sensación orgánica de MilkDrop surge de transformar una memoria visual compartida, interpolar movimientos y variar espacialmente la mezcla. Para los grafos heterogéneos de Guipper, la arquitectura de dos escenas de projectM/AVS resulta más directamente aplicable. El feedback heredado de MilkDrop conviene incorporarlo después, explícitamente y solo en nodos compatibles.

La investigación original no modificó el motor: los proyectos externos se descargaron y se leyeron, sin compilarlos ni ejecutarlos. Posteriormente se implementó la primera etapa en Guipper: captura de salida completa y fundido de sesión. Su alcance, límites y prueba gráfica se describen en [SESSION_TRANSITIONS.md](../tests/SESSION_TRANSITIONS.md). Las etapas restantes de este documento siguen siendo propuestas.

## Fuentes y reproducción

Winamp alojaba distintas visualizaciones. Aquí se estudia principalmente **MilkDrop**, y se contrasta con **AVS**; no se trata de la transición de audio entre canciones. [Ryan Geiss publica el código de MilkDrop 2.25c](https://www.geisswerks.com/about_milkdrop.html).

Repositorios descargados fuera de Guipper, en `/tmp/guipper-transition-research/`:

| Proyecto | Revisión analizada | Procedencia |
| --- | --- | --- |
| MilkDrop2 | `f05b0d811a87a17c4624170c26c93bac39b05bde` | [Repositorio oficial de SourceForge](https://sourceforge.net/p/milkdrop2/code/) |
| projectM | `1e7ef7803b69024d1e0656705670adda2ffac817` | [projectM-visualizer/projectm](https://github.com/projectM-visualizer/projectm) |
| AVS, port moderno | `1cee1f3d4f783b830538c3e4d2082a2c59521dee` | [grandchild/vis_avs](https://github.com/grandchild/vis_avs) |

Para repetir la descarga en un directorio de investigación:

```sh
git clone https://git.code.sf.net/p/milkdrop2/code milkdrop2
git clone https://github.com/projectM-visualizer/projectm.git projectm
git clone https://github.com/grandchild/vis_avs.git vis_avs
```

Luego hacer `git checkout` de las revisiones de la tabla. Las copias de `/tmp` son temporales; este documento conserva las referencias.

## 1. Qué hace MilkDrop2

### Memoria visual que continúa durante el cambio

En `src/vis_milk2/milkdropfs.cpp`, el render usa dos texturas intercambiables: lee `m_lpVS[0]`, escribe la siguiente imagen en `m_lpVS[1]` y al terminar intercambia ambas. Durante el cambio, los estados antiguo y nuevo participan en ese mismo circuito de feedback: la imagen que produce un frame vuelve a alimentar al siguiente.

Esto explica técnicamente cómo el movimiento nuevo puede deformar restos del anterior. Es una inferencia sobre el resultado visual a partir del circuito de render; no una comparación visual ejecutada en esta investigación. [Pipeline y cambio de buffers, líneas 1033–1219](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/milkdropfs.cpp#l1033).

### Mezcla de movimiento y de shaders

`ComputeGridAlphaValues()` calcula las coordenadas de muestreo de ambos estados y las interpola por vértice. El mismo peso se guarda como alfa para las pasadas de shaders. Sin shaders de warp, la interpolación de coordenadas permite resolver la transición en una pasada; con shaders se combinan las pasadas antigua y nueva, omitiendo regiones que no aportan.

La mezcla aparece tanto en el paso de deformación como en el de presentación. Por eso un swirl aplicado únicamente a dos imágenes terminadas reproduce solo una parte superficial del efecto. [Cálculo de coordenadas y alfa, líneas 1764–1946](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/milkdropfs.cpp#l1764).

### Una máscara espacial distinta por transición

`RandomizeBlendPattern()` elige entre:

- Barrido direccional: ángulo aleatorio y una franja de mezcla.
- Plasma: campo aleatorio suave, generado por subdivisión y normalizado.
- Radial: del centro hacia afuera o al revés.

El patrón se genera al empezar y sus coeficientes permanecen estables. Cada vértice calcula un peso `clamp(a * progreso + c, 0, 1)`. Unas regiones ya muestran el preset nuevo mientras otras conservan el anterior. El comentario del código explica que evitar una mezcla uniforme permite ahorrar trabajo al descartar regiones en las pasadas de shaders. Ese ahorro depende de su render por malla; un shader fullscreen de Guipper que muestree ambas imágenes no lo obtiene automáticamente. [Generación de patrones, líneas 7833–7968](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/plugin.cpp#l7833).

### Interpolación con significado conocido

`CState::StartBlendFrom()` interpola propiedades definidas por el motor: zoom, rotación, desplazamiento, decay, colores, ondas, bordes y otras. Algunas propiedades discretas cambian inmediatamente y otras tienen tratamiento particular. Las formas y ondas personalizadas usan pesos de aparición/desaparición.

No equivale a interpolar cualquier par de uniforms que compartan nombre. MilkDrop conoce el significado de sus propiedades. En Guipper, dos shaders pueden llamar `scale` a conceptos diferentes. [Estado y propiedades interpoladas, desde línea 705](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/state.cpp#l705).

### Preparación y tiempo

La carga suave usa un tercer estado candidato. `LoadPresetTick()` reparte la compilación de los shaders warp y composite entre distintos frames y activa el candidato al concluir. **No garantiza compilación sin bloqueos:** una compilación individual todavía puede tardar. El progreso usa tiempo transcurrido dividido por duración. Los valores iniciales del código son 1,7 s para cambios manuales y 2,7 s automáticos, configurables; no son una receta obligatoria para Guipper. [Carga escalonada, líneas 7993–8148](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/plugin.cpp#l7993).

## 2. projectM y AVS: alternativas más cercanas a nuestros grafos

### projectM actual

Mantiene un preset activo y otro entrante, renderiza ambos con la misma información de audio y mezcla sus texturas de salida mediante un compositor. Puede inicializar el nuevo con una imagen del anterior si `presetStartClean` está desactivado. Las transiciones incluyen mezcla simple, círculo, plasma, barrido, warp y zoom blur; reciben progreso, tiempo, audio y valores aleatorios estables por transición.

El código analizado fuerza la finalización del cambio anterior si llega otro. Para Guipper propongo una política distinta: partir de una captura de lo que se está viendo para evitar ese salto. [Ciclo de render y `StartPresetTransition`](https://github.com/projectM-visualizer/projectm/blob/1e7ef7803b69024d1e0656705670adda2ffac817/src/libprojectM/ProjectM.cpp#L288), [compositor](https://github.com/projectM-visualizer/projectm/blob/1e7ef7803b69024d1e0656705670adda2ffac817/src/libprojectM/Renderer/PresetTransition.cpp), [tipos de transición](https://github.com/projectM-visualizer/projectm/blob/1e7ef7803b69024d1e0656705670adda2ffac817/src/libprojectM/Renderer/TransitionShaderManager.cpp).

### AVS

El port moderno conserva una arquitectura con dos árboles de efectos y buffers separados. Ofrece continuar renderizando el preset saliente o conservar su imagen. También contiene preinicialización en un hilo y transiciones de fundido, desplazamiento, bloques, barridos y puntos.

La distinción **escena saliente viva / captura** es especialmente útil para controlar el costo en Guipper. La preinicialización de AVS trabaja con su render de CPU: no se debe trasladar literalmente a objetos OpenGL desde cualquier hilo. Estas observaciones corresponden al port inspeccionado, no a una comprobación de todas las versiones históricas distribuidas con Winamp. [Implementación de transiciones](https://github.com/grandchild/vis_avs/blob/1cee1f3d4f783b830538c3e4d2082a2c59521dee/avs/vis_avs/r_transition.cpp#L218).

## 3. Diagnóstico de Guipper

| Camino actual | Qué ocurre | Consecuencia |
| --- | --- | --- |
| Cambio de nodo activo | `updateTransition()` conserva punteros a los FBO de ambos nodos; el scheduler mantiene sus dependencias activas durante la mezcla. | Existe una base para dos ramas vivas dentro del mismo grafo. |
| Carga de composición `.xml` | `load()` construye candidatos, llama a `clear()`, instala los nuevos nodos y asigna `activerender` antes de `updateTransition()`. | La composición anterior ya no existe. Ambas fuentes de esa transición terminan apuntando al nuevo nodo activo. |
| Aplicar CUE | Usa una captura saliente y la nueva salida viva. | Hay un antecedente reutilizable para una transición de costo reducido. |
| Salida con FINAL | `drawLiveOutput()` presenta el composite FINAL. Su construcción en `JPboxgroup_quick_images.cpp` ya incorpora la transición del nodo activo como base y añade las capas. | La transición de nodo no desaparece, pero una transición de sesión debe conservar también las capas salientes de FINAL. |

Referencias locales:

- `src/JPbox/JPboxgroup_persistence.cpp`: `load()`, especialmente líneas 381–392; preparación de nodos mediante `setup()` antes del intercambio.
- `src/JPbox/JPboxgroup.cpp`: scheduler alrededor de línea 300; `armParameterMorph()` alrededor de 5467; `updateTransition()` alrededor de 5585; `drawLiveOutput()` alrededor de 2254; `clear()` alrededor de 7439.
- `src/JPbox/JPboxgroup_cue.cpp`: `cueApplySnapshotFbo`.
- `src/JPbox/JPboxgroup_quick_images.cpp`: construcción del composite FINAL, líneas 157–185.
- `src/JPutils/TransitionSR.cpp` y `.h`.
- `bin/data/shaders/private/mix.frag`, `transition_warp.frag`, `transition_dither.frag`.

Otros detalles relevantes:

- `TransitionSR` ya tiene duración, mezcla, warp y Bayer 4×4. Su warp deforma imágenes finales; no mezcla el circuito de feedback al estilo MilkDrop.
- El progreso acumula delta de tiempo limitado a 100 ms. Una pausa mayor alarga la duración real. Conviene separar el reloj de presentación de los límites usados para estabilizar simulaciones.
- El CPU aplica smoothstep y el shader warp vuelve a suavizar la mezcla. Conviene tener un único contrato para progreso lineal y curva.
- El morph de parámetros enlaza floats por nombre en los nodos superior saliente/entrante. Se debería habilitar por compatibilidad explícita de esquema o del shader.
- La carga candidata protege contra ciertos errores antes de `clear()`, pero construye nodos sincrónicamente y no espera explícitamente al primer fotograma válido de toda la composición.

## 4. Propuesta de implementación por etapas

### A. Cambio de composición confiable

Separar la preparación del candidato de su instalación. Estados sugeridos:

```text
Actual A → preparar B → primer frame válido de B → mezclar A/B → liberar A
                       ↘ error: conservar A y avisar
```

1. Conservar A durante lectura, validación y preparación de B. No llamar a `clear()` sobre A al iniciar la solicitud.
2. Separar lectura/parseo/decodificación de CPU del trabajo GPU; realizar compilación, subida de texturas y FBO en el contexto apropiado. Repartir operaciones GPU cuando sea posible. No prometer que un driver nunca bloqueará al compilar.
3. Definir disponibilidad por tipo de nodo: shader compilado, textura lista, primer frame de video/cámara o timeout explícito. Una imagen negra puede ser válida: no detectar disponibilidad por su color.
4. Comenzar el reloj de la transición al disponer de la salida entrante. Si falla la preparación, conservar A.
5. Primera entrega: captura de la salida completa A y B viva. Deja explícito que A se congela durante la mezcla. Esta etapa elimina el salto de reemplazo, pero no resuelve por sí sola las pausas de carga sincrónica.
6. Segunda entrega: A y B vivas con preparación escalonada. Extraer estado de render de la UI antes de duplicar `JPboxgroup`; no duplicar listeners de teclado/MIDI, captura de cámara ni actualización global de audio.
7. Una nueva solicitud durante una mezcla parte de una captura del resultado visible y prepara C. Limitar a un candidato pendiente y descartar candidatos obsoletos por identificador de solicitud.

La transición de sesión debe recibir las salidas completas de A y B, incluyendo sus capas FINAL, y publicar un resultado común antes de recortes y mapping de cada salida. CUE sigue siendo una previsualización independiente. Las salidas vinculadas directamente a nodos necesitan una política explícita para resolver identidades al cambiar de escena.

### B. Primera tanda visual

| Modo | Resultado buscado | Controles iniciales |
| --- | --- | --- |
| Fundido | Referencia limpia y predecible | Duración |
| Barrido suave | La nueva escena avanza desde un lado | Dirección, ancho del borde |
| Radial | Apertura/cierre desde un centro | Centro, sentido, ancho |
| Plasma | Regiones orgánicas que revelan la entrante | Escala, suavidad, semilla |

Mantener warp y dither existentes. Aleatorio debe elegir de un conjunto habilitado y fijar su semilla al inicio, sin regenerar la máscara cada frame. Probar inicialmente duraciones de 1,5–3 s; elegir el default mediante pruebas con las composiciones reales.

Un gestor independiente de OpenGL puede manejar estado, solicitudes, duración e interrupciones. El compositor GPU recibe dos texturas, progreso lineal y parámetros inmutables del efecto. Cada efecto decide su curva una sola vez. Preservar los identificadores numéricos existentes de tipos guardados.

### C. Movimiento orgánico y música

- Heredar imagen inicial únicamente en nodos que declaren una entrada de feedback/semilla compatible. No conectar automáticamente la salida anterior a todos los inputs de la nueva composición.
- Después, experimentar con feedback transitorio acotado en el compositor. Limitar intensidad y persistencia para evitar que el resultado se acumule sin control.
- Interpolar parámetros solo con correspondencias de significado y rango conocidas; separar ese morph de los valores guardados.
- Ofrecer inicio inmediato o cuantizado al próximo beat cuando haya un reloj musical confiable. El audio puede modular suavemente una deformación, pero el progreso principal debe seguir siendo monotónico y terminar a tiempo.

### D. Rendimiento y transparencia

Medir CPU, GPU y memoria durante preparación y mezcla, con CUE activo. Dos escenas más CUE pueden multiplicar el trabajo. Elegir previamente el modo vivo/captura según configuración o presupuesto medido; evitar cambiar de política inesperadamente a mitad del efecto.

Un FBO RGBA8 de 1920×1080 ocupa aproximadamente 7,9 MiB, sin mipmaps, MSAA ni profundidad. Dos imágenes y un resultado son unos 23,7 MiB **adicionales a los buffers internos de ambos grafos**. Estos últimos pueden dominar el costo.

Revisar la convención de alfa de extremo a extremo. El mix actual interpola RGBA directamente: probar imágenes transparentes y bordes para decidir si hace falta conversión a alfa premultiplicada dentro del compositor. Evitar alterar toda la biblioteca o introducir mezcla aditiva como solución automática. Evaluar mezcla en luz lineal solo después de comprobar los formatos y conversiones reales del pipeline.

## 5. Validación de la implementación futura

- Progreso y duración a 30/60/144 FPS, frame largo, pausa y duración cero; endpoints exactos A y B.
- A→B, A→B→C rápidamente, misma composición repetida y candidato cancelado.
- XML inválido, shader fallido, recurso faltante y media que tarda; la salida anterior debe permanecer disponible.
- Captura frente a dos escenas vivas; reloj y audio actualizados una vez por frame, sin reabrir innecesariamente dispositivos compartidos.
- Generativas, feedback, cámara, imagen, video, grupos anidados, FINAL y composición vacía.
- CUE y múltiples salidas: mismo progreso e imagen principal, recortes coherentes, sin punteros a FBO liberados.
- Máscaras deterministas, distintas relaciones de aspecto, resolución y transparencia.
- Medir peor frame y percentiles de tiempo además del FPS medio. Comparar grabaciones de las mismas parejas, con semillas fijas.

## 6. Licencias y alcance de reutilización

Las cabeceras de los archivos MilkDrop2 estudiados incluyen condiciones BSD de tres cláusulas; el port AVS contiene también ese aviso. projectM declara LGPL 2.1 o posterior en `COPYING`. Si se copia código concreto, conservar los avisos correspondientes y revisar sus dependencias por archivo. La propuesta inicial es implementar los mecanismos en la arquitectura propia, sin incorporar un motor DirectX o toda la biblioteca projectM. [Aviso MilkDrop](https://sourceforge.net/p/milkdrop2/code/ci/f05b0d811a87a17c4624170c26c93bac39b05bde/tree/src/vis_milk2/plugin.cpp#l1), [aviso projectM](https://github.com/projectM-visualizer/projectm/blob/1e7ef7803b69024d1e0656705670adda2ffac817/COPYING), [aviso AVS](https://github.com/grandchild/vis_avs/blob/1cee1f3d4f783b830538c3e4d2082a2c59521dee/LICENSE.TXT).

**Siguiente paso recomendado:** implementar y probar el ciclo de cambio de composición con captura de salida completa y fundido correcto. Luego preparar escenas sin saltos y añadir plasma, radial y barrido. El feedback compartido queda como experimento posterior, medido y optativo.
