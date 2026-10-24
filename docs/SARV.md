# SARV — continuidad de desarrollo en Linux

Actualizado: 2026-10-24. Estado: primera implementación funcional probada en Windows; pendiente de compilar y validar en Linux.

## Objetivo del usuario

Crear una caja independiente llamada **SARV** en Guipper que reproduzca la lógica, los parámetros y el aspecto del proyecto TouchDesigner `Kinect_SARV`: superficie blanca de profundidad y partículas rojas que se desprenden formando filamentos. Mejorar esta caja sin sustituir Partículas Kinect.

El usuario necesita conservar el uso de Kinect con otros programas en Windows, en particular TouchDesigner. No cambiar el controlador Microsoft por uno exclusivo de libusb para resolver SARV. En Linux, conservar el backend libfreenect2 existente.

## Cómo retomar

1. Leer este documento y revisar el código y el estado Git real del checkout.
2. Transferir también los archivos nuevos: al escribir este documento, los fuentes y shaders SARV todavía figuraban como **untracked**. No hay un commit ni push realizado en esta tarea; cambiar de equipo o hacer pull no garantiza que estén allí.
3. Preparar openFrameworks 0.12.1, addons y libfreenect2 según el README.
4. Compilar con Make, abrir SARV con Shift+V y validar primero captura/orientación/recorte, antes de cambiar la estética.
5. Priorizar la fidelidad del ruido y de la memoria temporal. Comparar con una referencia de TouchDesigner, no solamente con la caja Partículas anterior.

### Compilación Linux

Desde la raíz del proyecto, ubicado en `apps/myApps/Guipper` dentro de openFrameworks:

```bash
# Verifica si la compilación habilitará Kinect.
pkg-config --modversion freenect2

make Release -j2
./bin/launch-guipper.sh
```

Si falta libfreenect2, revisar/ejecutar `scripts/install_kinect2_linux.sh` como describe el README, instalar sus reglas udev y verificar el dispositivo con Protonect. Cerrar Protonect antes de abrir Guipper: no dar por hecho que dos procesos libfreenect2 pueden compartir el dispositivo. Dentro de Guipper se comparte una única captura.

`config.make` detecta freenect2 mediante pkg-config, define `KINECT2` y agrega la biblioteca. Sin ella puede compilar, pero no habrá captura. `OF_ROOT` apunta a `../../..`; addons: `ofxNDI`, `ofxOsc`, `ofxMidi`. Revisar también el parche de cámara de openFrameworks documentado en README.

Para seleccionar GPU o activar el perfilador existente:

```bash
GUIPPER_GPU=nvidia ./bin/launch-guipper.sh
GUIPPER_PROFILE=1 ./bin/launch-guipper.sh
```

**Qbs pendiente:** `guipper.qbs` tiene una lista explícita de fuentes y todavía no se agregaron `jp_box_sarv.cpp/.h`. Si se usa Qbs, incorporarlos antes de compilar. Visual Studio sí incluye el `.cpp`. Make usa el descubrimiento de fuentes de openFrameworks; verificar que el archivo nuevo aparezca en su compilación.

Los shaders SARV declaran GLSL 330 y usan texturas rectangulares RGBA32F. El README describe un contexto OpenGL 3.2: verificar la versión real del driver/contexto en Linux y la compilación de los shaders; no asumir compatibilidad sólo porque compiló C++.

## Uso y parámetros

En NODES, **Shift+V** crea SARV. **Shift+K** sigue creando Partículas Kinect. La caja adquiere la profundidad compartida directamente, sin necesitar una conexión desde otra caja Kinect. Puede añadirse a FINAL desde el inspector.

| Parámetro | Inicial | Rango actual | Función |
|---|---:|---:|---|
| `time_slices` | 15 | 1–60 | Cuadros de memoria temporal; se redondea a entero |
| `kinect_depth` | 2 | 0–8 m | Límite máximo de profundidad conservada |
| `min_points_scale` | 0.0015 | 0.0001–0.02 | Escala mínima de la superficie |
| `max_points_scale` | 0.0015 | 0.0001–0.02 | Escala máxima de la superficie |
| `spheres_cubes_points` | 0 | 0–2 | Esferas / cubos / puntos; también tiene pestañas |
| `particles_res_mult` | 2 | 1–2 | Multiplicador entero de la resolución de simulación |
| `particle_life_duration` | 1 | 0.05–10 s | Duración de vida |
| `particle_life_variance` | 0 | 0–1 | Variación del avance de edad |

Multiplicador 1: 512×424 = 217.088 posiciones. Multiplicador 2: 1024×848 = 868.352 posiciones. El contador del inspector indica **slots de simulación**, no partículas visibles ni cuadros nuevos de sensor. Las escalas sólo afectan la capa de superficie; las partículas desprendidas se dibujan a un píxel.

**Corrección importante:** en el Threshold TOP de referencia, comparator=`greater` elimina los valores mayores al umbral. Se verificó leyendo muestras de `null2` y `thresh2`: profundidad 1.108 → máscara 1; profundidad 3.259 → máscara 0, con umbral 2. No invertirlo nuevamente. Guipper conserva `0 < z < kinect_depth`; el tratamiento exacto de igualdad en TD no fue verificado.

## Mapa de archivos

| Archivo | Responsabilidad |
|---|---|
| `src/JPbox/jp_box_sarv.h` | Clase, captura, historial, texturas y estado GPU |
| `src/JPbox/jp_box_sarv.cpp` | Parámetros, reconstrucción de posiciones, pasos y render |
| `bin/data/shaders/private/sarv_update.vert/.frag` | Evolución y reinicio de partículas |
| `bin/data/shaders/private/sarv_render.vert/.frag` | Proyección, paleta y superficie |
| `src/JPbox/jp_box_factory.h/.cpp` | Kind::Sarv, directorio `sarv` y construcción |
| `src/JPbox/jp_box.h` | Tipo SARVBOX agregado al final del enum |
| `src/JPbox/JPboxgroup.cpp` | Nombre visible SARV |
| `src/ofApp.cpp` | Atajo Shift+V, protegido frente a campos y modificadores |
| `src/JPgui/jp_slider.cpp` | Cuatro decimales para ambas escalas SARV |
| `guipper.vcxproj` | Fuente SARV incluida en Windows |
| `src/JPutils/jp_persistence_test.cpp` | Caso SARV agregado a clasificación de fábrica; suite no ejecutada |
| `tests/SARV.md` | Nota breve de implementación y validación Windows |

Los shaders se cargan desde `jp::AppPaths::current().bundle / shaders/private`, no desde una copia antigua del perfil de usuario. Transferir `bin/data` junto con los fuentes. No reemplazar los shaders privados de Partículas por los de SARV.

## Implementación actual

- Captura con `JPbox_kinect2::acquireSharedCapture()`, `acquireRawDepth()` y `depthIntrinsics()`; profundidad en milímetros, convertida a metros. Formato esperado fijo de 512×424.
- Historial de referencias a cuadros inmutables. Se agrega una referencia en cada paso de 60 Hz, incluso si el sensor de 30 Hz todavía no produjo otro cuadro.
- `buildSource()` recorre 512×424 píxeles en CPU, calcula una rampa vertical espejada y deformada con `ofNoise`, elige un cuadro del historial por píxel, filtra profundidad y reconstruye XYZ. Sube una textura RGBA32F.
- Posición final actual: `x=-(px+0.5-cx)/fx*z+0.07`, `y=-(py+0.5-cy)/fy*z+0.21`, `z_world=1-z`. Se comprobó visualmente el encuadre en Windows; volver a verificar las convenciones del backend Linux.
- Píxeles inválidos/fuera de rango reciben XYZ=999. Tras más de 0.5 s sin profundidad nueva, la fuente se invalida.
- Dos FBO RGBA32F alternados guardan XYZ y edad normalizada en alfa. Cada slot se reinicia en el píxel correspondiente de la fuente; no hay emisor aleatorio por cantidad de nacimientos.
- Paso fijo de 1/60 s, con dt de actualización limitado a 0.1 s. Edad inicial aleatoria para escalonar los reinicios. La simulación limita la recuperación de tiempo tras un bloqueo; no promete mantener tiempo real con sobrecarga sostenida.
- Por paso: `p = p * (1.007,1.009,1) + (0,0.006,-0.01) + ruido1 + ruido2`.
- Campo 1: período 3, amplitud .008, avance Z .2/s. Campo 2: período .7, amplitud .003, avance Z .6/s, desplazamiento de semilla 1555. Tres bandas de ruido con pesos 1, .7 y .49, normalizados por 2.19.
- Al llegar a edad 1, posición de fuente y edad 0. La varianza multiplica el incremento de edad.
- Cámara equivalente: posición `(0,.14,1.8)`, FOV horizontal 45°, cerca .1; proyección calculada directamente en el shader. Fondo negro, prueba y escritura de profundidad, sin blending para ambas capas.
- Paleta aplicada a `1-edad`, con interpolación lineal entre cinco colores.

Paleta leída de `/project1/colour_lovers_picker/out2`, título **american idiot**:

```text
(0,          0,          0)
(.62109375,  .06640625,  .10546875)
(.69140625,  .0859375,   .13671875)
(.16015625,  .171875,    .21484375)
(.796875,    .796875,    .796875)
```

## Referencia TouchDesigner

Se inspeccionó por MCP el proyecto abierto **Kinect_SARV.12.toe**, raíz `/project1`, a 60 Hz, TouchDesigner 2025.32460. Los ocho controles están en `constant7`. El proyecto abierto tenía cambios sin guardar; no se guardó ni sobrescribió desde esta tarea. No confundir sus valores con los del `.toe` original.

Ubicaciones del equipo Windows, **no portables ni garantizadas dentro de Git**:

- Proyecto original: `C:/Users/nicoo/Documents/VJ/paid/Kinect_SARV.toe`.
- Proyecto abierto: `C:/Users/nicoo/Documents/VJ/paid/Kinect_SARV.12.toe`.
- Extracción de una copia del original: `dist/sarv-reference/Kinect_SARV.toe.dir/project1`.
- Captura temporal del resultado: `C:/Users/nicoo/AppData/Local/Derivative/TDMCP/cache/13316/null7.png`; el caché puede sobrescribirse.

Si se necesita una comparación nueva en Linux, llevar una exportación autorizada del proyecto/una captura de referencia. El MCP de TouchDesigner quedó registrado localmente en Windows en `http://127.0.0.1:13316/mcp`; ese localhost no conecta automáticamente desde Linux a la máquina Windows.

### Red relevante

1. `kinect1` entrega `depthpointcloud`; `null10` alimenta `tex3d1` (15 cuadros).
2. `ramp1`: vertical, período .03, extensión mirror; blanco en posiciones 0 y .41057938, negro en .59445846 y 1.
3. `noise3` deforma la rampa mediante `displace1`: pesos X/Y .05, canales rojo/azul; avance Z .1/s. `blur1`: preshrink 2 y size 2.
4. `tima1`: caché + rampa deformada, blackoffset -15 y whiteoffset 0; salida `null2`.
5. `thresh2`: blue/Z, comparator greater, threshold `kinect_depth`. Transformación de posiciones: traslación `(-.07,.21,-1)` seguida de rotación Y -180°. Máscara y `constant1` sustituyen lo excluido por 999. Salida `null5`/`pos`.
6. Superficie `geo1`: posiciones de `pos`, rotaciones XYZ aleatorias de `noise1` (amplitud 180), escalas de `noise2` remapeadas entre min/max; sphere/box/point y materiales phong/point sprite.
7. `select1` → reemplazo NaN por 0 → `res1` a 1024×848. `feedback1` guarda posiciones de `null9`.
8. Feedback → `pointtransform3` con la escala/traslación por paso indicada arriba. `noise5` y `noise8` reciben también las posiciones como mapa de coordenadas; sumar ambos campos a la posición transformada.
9. Componente `life`: edad inicial aleatoria, avance según duración, ruido de varianza, reinicio por píxel. Sus posiciones de reinicio se componen sobre las evolucionadas → `null9` → `pos_particles`.
10. Invertir edad → lookup de paleta → color de `geo3`; partículas de tamaño 1 con depth test/write. Cámara `cam1`; salida de referencia `null7`, render 1920×1080.

## Diferencias conocidas y orden de mejora

1. **Ruido y filamentos:** Guipper usa value noise interpolado con hash seno; TD usa simplex3d en los dos campos. No son equivalentes por semilla ni por distribución. Reproducir mejor el campo y confirmar la interpretación de períodos, coordenadas, armónicos y normalización antes de retocar amplitudes para compensar. La variante de vida tampoco reproduce el perlin4d de TD.
2. **Memoria temporal:** falta el blur de la rampa; desplazamiento aproximado, elección del cuadro más cercano y remuestreo nearest. Verificar el filtrado de Time Machine y Resize TOP y la correspondencia exacta de blanco/negro y edad del caché. `time_slices` es cantidad de cuadros, no cantidad de bandas.
3. **Geometría:** el modo esferas son discos sombreados, cubos son puntos cuadrados y puntos son píxeles. No hay mallas instanciadas ni rotaciones XYZ aleatorias. Implementar geometría real o una aproximación visual mejor, midiendo el coste. La escala variable también usa un hash en vez del campo de TD.
4. **Rendimiento:** medir `buildSource()` y la subida de textura, coste real GPU y FPS con resolución x1/x2. Considerar caché de profundidad y selección temporal en GPU. No confundir el contador de slots ni el perfilador CPU con throughput de captura o tiempo GPU.
5. **Robustez:** agregar pruebas sintéticas para el umbral, orientación, vida/reinicio, resize, valores extremos y pérdida/reconexión del sensor. Verificar restauración del estado GL, pausa, borrado/undo/redo y liberación de la captura.
6. **Persistencia y distribución:** probar guardar/cargar los ocho controles, fábrica en ambos contextos, paquete Linux con los cuatro shaders y soporte Qbs.

## Validado y pendiente

Validado el 2026-10-24 en Windows:

- Build Release x64 correcto y ejecutable abierto desde este checkout.
- Creación mediante Shift+V; ocho controles, escalas legibles a cuatro decimales.
- Captura y render en vivo mientras TouchDesigner seguía abierto.
- Contador 868.352 / 15 cuadros, superficie blanca, partículas rojas, corte de profundidad y orientación corregidos mediante comparación con TD.
- Cambio entre las tres pestañas; vuelta al modo esferas y valores originales.
- Sin errores de los shaders SARV en los logs; `git diff --check` correcto.

No validado: compilación/ejecución Linux, FPS o coste GPU medido, equivalencia píxel a píxel, prueba prolongada, suite completa de persistencia, cambio de multiplicador/lifetime bajo carga. El caso de fábrica se agregó al código de pruebas pero la suite no se ejecutó.

Persisten pérdidas momentáneas de disponibilidad de Kinect. En trabajo anterior también ocurrieron con un probe del SDK fuera de Guipper, con conexión directa USB 3 según el usuario. No atribuir automáticamente cada corte al efecto SARV ni declarar resuelto el hardware. Tampoco garantizar acceso simultáneo a la Kinect entre procesos/plataformas por haber funcionado durante esta prueba Windows.

## Cambios previos que deben conservarse

El checkout ya tenía cambios de Kinect/Partículas anteriores a SARV: backend Windows SDK en `src/JPbox/jp_box_kinect2.cpp`, nuevo `src/JPutils/jp_kinect_sdk.h`, cambios de Partículas y sus pruebas, y notas/probe Windows en `tests/WINDOWS_KINECT.md` y `tests/windows_kinect_probe.cpp`. No descartarlos ni sobrescribirlos al transportar SARV. Revisar `git status` y separar lo que se desea transferir; este documento no equivale a haber sincronizado el código.

## Pedido sugerido para continuar

> Leé docs/SARV.md y revisá el estado actual. Compilá Guipper en Linux, verificá Kinect y la caja SARV con Shift+V. Conservá Partículas como caja independiente y los dos backends de captura. Mejorá primero la fidelidad de los filamentos y la memoria temporal respecto de Kinect_SARV de TouchDesigner, midiendo rendimiento. No declares copia exacta ni soporte validado donde todavía haya aproximaciones o pruebas pendientes.
