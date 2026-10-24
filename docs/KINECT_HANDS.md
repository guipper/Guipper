# Seguimiento: público, manos y cuerpo RGB (Linux)

## Caso de uso principal: público en fiestas oscuras

La Kinect apunta al público durante fiestas de música electrónica, con poca
luz visible y varias personas. Este es el escenario prioritario del producto.
Las pruebas de una persona frente a una webcam iluminada solo validan esa
modalidad; no validan el uso frente al público en oscuridad.

El modo **Publico** utiliza profundidad, sin depender del RGB, para medir
movimiento colectivo, ocupación por zonas, centro de actividad y distancia
de las superficies detectadas,
con exclusión del escenario/fondo y calibración del área útil. Estas señales
no equivalen a contar personas ni a identificar articulaciones individuales.
Cuerpo RGB queda como modalidad complementaria que requiere luz visible.

Validar con la ubicación y distancia reales de la Kinect, varias personas,
oclusiones y cambios de iluminación del evento. El seguimiento fino de dedos
en IR sigue siendo experimental; no se debe prometer su funcionamiento a
distancia ni presentar el modelo RGB como validado para infrarrojo.
El rango inicial de Público es **3–4,5 m**, según la ubicación prevista por el
usuario. Ver la sección Público para puesta en marcha y límites.

Cada parámetro puede elegir su fuente. Kinect RGB, Kinect infrarrojo y webcams
pueden funcionar simultáneamente. Manos y cuerpo usan MediaPipe local para una
persona, con hasta dos manos o un cuerpo por detector, sin cajas nuevas. Manos y cuerpo
pueden compartir la misma cámara con procesos de inferencia independientes.

**El modo infrarrojo es experimental.** La entrada IR funciona sin luz visible,
pero el modelo de dedos está pendiente de validación a 1,2–2 m. Convertir IR a
RGB8 no convierte al modelo en un detector entrenado para infrarrojo.

## Instalación

```sh
./scripts/install_hand_tracking_linux.sh
make Release JOBS=-j2 -j2
```

Linux, Python 3.12+, pip/venv, curl y sha256sum. Kinect requiere libfreenect2,
alimentación y USB 3; una webcam puede usarse sin instalar libfreenect2.
El entorno aislado vive en `$XDG_DATA_HOME/Guipper/hand-tracking`, por defecto
`~/.local/share/Guipper/hand-tracking`; `GUIPPER_HAND_RUNTIME` permite otro destino.
Versiones fijadas en `scripts/hand_tracking/requirements.lock`, modelos verificados
con SHA-256. El ejecutable y `worker.py` deben usar el protocolo **2**; reinstalar
el runtime al actualizar desde la versión inicial. No se descarga nada al abrir
Guipper ni se reutiliza el seguimiento corporal descartado.

## Uso

1. Pulsar el **icono de persona** junto a un parámetro o abrir **SETTINGS → Seguimiento**.
2. Elegir **Kinect RGB**, **Kinect infrarrojo** o **Cámara de PC**. En esta última,
   seleccionar un dispositivo de la lista; **Actualizar cámaras** vuelve a buscar.
3. Elegir **Manos** o **Cuerpo RGB**, señal y rango de salida del parámetro.
   Cuerpo requiere luz visible y Kinect RGB o una cámara de PC; no admite IR.
   Los botones de mano, señal y punto avanzan con clic izquierdo y retroceden
   con clic derecho; al llegar a un extremo, continúan desde el otro.
4. Pulsar **Activar vínculo** e **Iniciar esta fuente**. Cada fuente arranca
   explícitamente; cargar una composición deja todas detenidas.

Ejemplo: ANALOG COLOR → pixel size → Kinect infrarrojo → mano derecha → Pinza,
con salida 1–32. Otro parámetro puede usar X de la webcam al mismo tiempo.

Iniciar/detener afecta a todos los vínculos que comparten esa fuente, cuyo número
se muestra en el panel. **Reconectar** Kinect reinicia ambas modalidades porque
comparten dispositivo; las webcams conservan sus sesiones. Detener seguimiento
no cierra una cámara que CAMARITA u otra caja todavía utiliza.

La imagen no está espejada. Izquierda/derecha es la mano anatómica; X crece hacia
la derecha de la imagen e Y hacia arriba. La conversión de lateralidad de cámaras V4L2 es independiente de
la calibración de Kinect; no se invierten los píxeles para cambiar la etiqueta.
La reducción de cámaras HD usa interpolación bilineal CPU y conserva proporciones.
La textura de diagnóstico se reasigna al cambiar de resolución o de sesión,
incluso si la nueva imagen tiene igual o menor tamaño.

El selector de fuente pertenece al
parámetro abierto, no a toda la composición. El panel es flotante y no modal.

## Señales y estados

- **Pinza:** distancia pulgar–índice dividida por ancho de palma; entrada 0,15–1.
- **Apertura:** extensión angular normalizada de los cuatro dedos largos.
- **Posición X/Y:** palma o cualquiera de los 21 puntos.
- **Distancia:** metros medidos por Kinect, nunca Z estimado por MediaPipe.
  Entrada inicial 0,7–1,5 m, editable para el rango de trabajo.

La webcam no ofrece distancia en metros. Un vínculo guardado que necesita Z
conserva su configuración y muestra **Distancia no disponible** si se cambia a
webcam. Las otras señales siguen funcionando sin profundidad.

Suavizado inicial de 100 ms y zona muerta del 1 %. Los resultados vencen a los
250 ms. Por defecto las pérdidas conservan el último valor; también se puede
elegir volver suavemente al valor base o desactivar el vínculo después de
0,5 segundos sin señal válida. Antes del primer resultado se mantiene la base.
Detener la fuente conserva el vínculo; reactivarla requiere Iniciar. Cambiar de fuente o reiniciarla descarta el filtro de la sesión anterior.
MIDI, OSC y edición manual no compiten con un vínculo activo.

El diagnóstico muestra la imagen que entra al detector, esqueleto, región de Z,
resolución de entrada al detector, FPS, duración, edad y descartes. La edad se
mide desde la recepción del cuadro decodificado en el capturador: no incluye
exposición ni toda la decodificación USB y no es latencia física extremo a extremo.
La simulación está identificada y permite ensayar pérdidas por fuente.

## Público: control con profundidad en oscuridad

**Público** analiza la profundidad nativa de Kinect v2 en Linux. No necesita
RGB, Python ni modelos de MediaPipe. Su captura IR/profundidad sigue llegando
cuando no hay un cuadro RGB nuevo; comparte una sola apertura USB con las cajas
Kinect y con manos IR. El resultado describe superficies, no personas individuales.
El LiDAR de un iPhone también puede alimentar Público: ver
[iPhone LiDAR](IPHONE_LIDAR.md).

1. Abrir el icono de seguimiento de un parámetro y elegir **Publico**.
2. Activar el vínculo y pulsar **Iniciar esta fuente**. El rango inicial es
   **3–4,5 metros**; el extremo superior se excluye del área útil.
3. Abrir **Area y fondo** y arrastrar sobre la imagen para delimitar la pista.
   Ajustar distancia mínima/máxima para excluir escenario, techo y zonas ajenas.
4. Con la pista vacía y la Kinect fija, pulsar **Aprender fondo · pista vacia · 2 s**.
   Activa la exclusión de superficies fijas. Si ya hay público, usar inicialmente
   área y distancias; aprenderlo como fondo excluiría también esas superficies.
5. Volver al vínculo, elegir señal y **Toda el area / izquierda / central / derecha**.
   Ajustar los extremos de entrada, o usar **Medir mi recorrido**, y el rango de
   salida del parámetro. Clic derecho recorre los selectores hacia atrás.

Señales disponibles:

| Señal | Significado |
| --- | --- |
| Movimiento | Actividad de profundidad en la zona, normalizada de 0 a 1. |
| Ocupación | Fracción del área de imagen ocupada por superficies aceptadas, de 0 a 1. No es cantidad de personas. |
| Centro X/Y | Centro de las superficies aceptadas; X a la derecha, Y hacia arriba de la imagen. |
| Distancia mediana | Mediana de profundidad en metros de esas superficies. |
| Actividad X/Y | Centro del movimiento; sin movimiento válido conserva la política de pérdida del vínculo. |

Ejemplo: `pixel size ← Público / Movimiento`, entrada 0–0,15 y salida 1–32.
Un segundo parámetro puede usar ocupación de la zona izquierda y un tercero
centro X. Las zonas son tercios de la región elegida; las coordenadas X/Y se
expresan respecto de la imagen completa. Calibrar el rango al montaje real.

La previsualización proviene de **profundidad**, no de RGB: cerca claro, lejos
oscuro, negro sin datos. Cian marca superficies aceptadas y oro movimiento.
**Datos validos** muestra cobertura del sensor en la zona, incluyendo superficies
válidas fuera del rango elegido. Se exige al menos un 50 % de cobertura para
emitir la señal; un hueco no se interpreta como una persona ni como movimiento.
Sin superficies aceptadas, ocupación y movimiento pueden valer cero; centro y
distancia quedan sin señal. Los resultados vencen a los 250 ms.

La profundidad se reduce a una grilla de 128×106 con consenso local de 3×3
píxeles para rechazar puntos aislados. El movimiento compara profundidad válida
en una ventana cercana a 100 ms, adaptable entre 80 y 250 ms según la cadencia
de captura: necesita datos comparables en al
menos la mitad de la zona. Los límites **Velocidad plena** y **Ruido en 100 ms**
ajustan la respuesta; menor velocidad plena aumenta sensibilidad. La señal
resume cambios de profundidad y de contorno, no velocidad corporal real ni
flujo óptico completo. El movimiento lateral sobre una superficie uniforme a
igual profundidad puede generar poca respuesta.

El fondo requiere al menos doce cuadros durante dos segundos. Se conservan
muestras estables por píxel; las superficies nuevas deben estar al menos 15 cm
por delante del fondo. Datos insuficientes o una interrupción durante el
aprendizaje dejan un estado de error recuperable. Repetir con la pista vacía.
Mover la Kinect requiere borrar o reaprender el fondo. Mientras se aprende,
los parámetros mantienen su valor sin activar la política de pérdida.

**Área, distancias, sensibilidad, zona y exclusión de fondo se guardan por vínculo**,
con copia, grupos y undo/redo. El mapa de fondo se comparte entre los vínculos
Público durante la sesión; no se guarda en XML. Se descarta al detener, reconectar
o liberar el worker por falta de consumidores. Un vínculo guardado con exclusión
activada pedirá aprender el fondo antes de emitir valores. Aprender/borrar el mapa
es una acción de sesión; undo conserva solamente la configuración del vínculo.

El análisis corre en segundo plano y conserva el último resultado. Los vínculos
con la misma área/configuración comparten análisis; las cuatro zonas se calculan
juntas. Se admiten hasta 64 configuraciones activas distintas por fuente. Render
solo toma resultados y carga texturas. La simulación usa el mismo motor de
profundidad y permite comprobar pérdida y recuperación sin cámara.

Pruebas reproducibles en perfiles temporales:

```sh
make -C tests crowd_tracking_tests && ./tests/crowd_tracking_tests
python3 tests/hand_graphics_test.py
# Kinect real, tres parámetros; 90 s activos y 30 s de referencia:
GUIPPER_TRACK_CROWD=1 GUIPPER_HAND_BENCH_SECONDS=90 \
  python3 tests/hand_graphics_test.py --real
```

El harness real guarda métricas y una captura del panel cada 600 cuadros bajo
`/tmp`; no modifica composiciones. La prueba de una habitación no valida una
fiesta: comprobar oscuridad, varias personas, oclusiones, movimiento colectivo,
ubicación fija y cobertura a 3–4,5 m en el montaje real antes de usarlo en vivo.

## Cuerpo RGB y editor del vínculo

El detector Pose Landmarker Lite entrega 33 puntos para **una persona**. Se
utilizan coordenadas de imagen, no una reconstrucción 3D ni distancia en metros.
Trabajar de frente a la cámara: el giro de perfil reduce el ancho de hombros
en la imagen y altera la normalización de separación de muñecas.
Los puntos fuera de cuadro o con presencia/visibilidad inferior a 0,6 no mueven
el parámetro. Deben verse los puntos requeridos por la señal elegida:

- **Torso X/Y:** centro de ambos hombros y caderas, rango inicial 0–1.
- **Separación de muñecas:** distancia entre muñecas dividida por el ancho de
  hombros, con corrección de proporciones; rango inicial 0–3.
- **Altura de brazos:** altura media de las muñecas respecto de las caderas,
  normalizada por la altura del torso; caderas = 0, hombros = 1, rango 0–2.
- **Punto X/Y:** cualquiera de los 33 puntos; X a la derecha e Y hacia arriba.

Hacer clic sobre un punto del esqueleto lo selecciona para controlar X/Y.
**Centrar posición actual** coloca la señal en el centro del rango de entrada.
**Medir mi recorrido · 5 s** registra extremos válidos del movimiento y los aplica
como una sola edición deshacible. Requiere al menos doce muestras y un recorrido
mayor que 0,01; se cancela al cambiar fuente, señal o configuración.

El resumen bajo cada parámetro muestra cámara, detector y señal; pulsarlo abre
su vínculo. **Diagnóstico** despliega las métricas y el gráfico sin cambiar la
fuente. El panel permanece abierto junto al inspector. Los selectores cíclicos
avanzan con clic izquierdo y retroceden con clic derecho.

Modelo oficial fijado: `pose_landmarker_lite/float16/1`, SHA-256
`59929e1d1ee95287735ddd833b19cf4ac46d29bc7afddbbf6753c459690d574a`.
La instalación actualiza ambos modelos y el worker sin reutilizar las cajas
experimentales retiradas. Referencia: [Pose Landmarker](https://developers.google.com/edge/mediapipe/solutions/vision/pose_landmarker/python).

Prueba de cuerpo en un perfil temporal, con tres parámetros desde la misma cámara:

```sh
make -C tests tracking_body_tests && ./tests/tracking_body_tests
GUIPPER_TRACK_BODY=1 GUIPPER_HAND_CAMERA=/dev/v4l/by-id/IDENTIDAD-DE-LA-CAMARA \
  python3 tests/hand_graphics_test.py --real
```

La identidad debe corresponder a la cámara elegida. Mostrar hombros, caderas y
muñecas; mover el torso, separar/acercar muñecas y levantar/bajar brazos. Comprobar
pérdida/regreso al cuadro y los lados anatómicos. No se garantiza conservar la
identidad de una persona si entra otra: esta entrega supone una sola persona.

## Implementación y compatibilidad

Una captura por dispositivo y un proceso por combinación de fuente y detector. Varios
parámetros de una misma fuente comparten inferencia y snapshot por actualización.
Cada proceso tiene cola del último cuadro, límites de espera y generación de
cancelación; sus respuestas no pueden actualizar otra sesión. Sin consumidores
ni diagnóstico abierto, se liberan proceso y participación en captura.

Kinect RGB se reduce a 960×540 y usa profundidad registrada de 1920×1080,
excluyendo las filas extra de `bigdepth`. IR usa 512×424 y profundidad nativa en
las mismas coordenadas. No espera al RGB. Se normaliza la intensidad logarítmica
entre percentiles 1 y 99, suavizando límites durante 500 ms. El parche interior
de palma se adapta a su tamaño y rechaza muestras ausentes o dispersas.

Las webcams comparten captura con CAMARITA. Se conserva un formato ya abierto;
una nueva captura solicita 1280×720 a 30 FPS, y el detector recibe como máximo
960×540 conservando proporciones. Abrir, leer y cerrar ocurre en segundo plano;
las texturas solo se crean y actualizan en render. Dispositivos ausentes no se
sustituyen por otro índice de cámara. Se prefieren identificadores `/dev/v4l/by-id`
y luego `/dev/v4l/by-path`; sin identidad persistente se identifica la sesión.

Se conserva `movtype=7` y el nodo XML `kinect`, con versión 4 y campos `source`,
`device`, `detector`, `loss` y configuración `crowd`. Los vínculos antiguos se interpretan como manos
Kinect RGB, con pérdida configurada para mantener el valor. Copia, grupos,
parámetros expuestos, undo/redo y transiciones conservan configuración sin
persistir procesos, imágenes ni valores filtrados. Windows/macOS pueden cargar
la composición, pero el seguimiento de esta entrega es solo Linux.

## Pruebas reproducibles

```sh
g++ -std=c++17 tests/hand_tracking_tests.cpp -o /tmp/hand-tests
/tmp/hand-tests
python3 tests/hand_worker_test.py
python3 tests/hand_graphics_test.py
# IR, sin guardar imágenes de la cámara:
GUIPPER_HAND_IR=1 python3 tests/hand_graphics_test.py --real
# RGB + IR + webcam, tres parámetros y CAMARITA compartida:
GUIPPER_HAND_MULTI=1 python3 tests/hand_graphics_test.py --real
```

Las pruebas gráficas usan copias y perfiles bajo `/tmp`. La prueba real mide
600 segundos activos y 30 de referencia. Guarda CSV de métricas; si se indica
`GUIPPER_HAND_CAMERA`, también guarda una captura del panel cada 600 cuadros
para comprobar la imagen real. Todos esos archivos quedan en el perfil temporal.
`GUIPPER_HAND_BENCH_SECONDS=30` permite una comprobación breve;
`GUIPPER_HAND_CLOSE_ACTIVE=1` comprueba cierre con captura activa.

Cerrar otras aplicaciones/instancias que estén usando la Kinect antes de la
prueba. Para IR, probar veinte ciclos de pinza por mano a 1,2, 1,6 y 2 m, con luz
y oscuridad. Objetivo: 18/20 ciclos utilizables y seguimiento válido en el 90 %
de los intervalos visibles sin oclusión. Medir por fuente: objetivo 15 resultados/s
y edad p95 inferior a 200 ms; registrar aparte captura y render. Probar también
apertura, X/Y, inmovilidad, cruces, pérdida/regreso y reconexión USB.

Compilar y pasar simulaciones no valida los dedos en oscuridad. Si IR no supera
la prueba, conservar la etiqueta experimental; no sustituirlo por RGB o palma.

Referencias: [Hand Landmarker](https://developers.google.com/edge/mediapipe/solutions/vision/hand_landmarker/python),
[registro libfreenect2](https://openkinect.github.io/libfreenect2/classlibfreenect2_1_1Registration.html).

## Validación de la ampliación IR/webcam

Pruebas de núcleo: **PASS** para mapeo, pérdidas, vencimiento, asociación de
manos, proporciones variables, profundidad nativa IR y normalización temporal.
El modelo instalado procesa cuadros RGB8 de 960×540, 640×480 y 512×424, verifica
la identidad del cuadro y termina limpiamente al cerrar la entrada.

Regresiones en perfil aislado: **PASS** para fuentes simultáneas, aislamiento de un proceso
bloqueado, respuestas de otro cuadro rechazadas, cambio de fuente, ausencia de Z,
XML antiguo/nuevo, copia, undo/redo, grupos y parámetros expuestos. El cierre del
harness usa la misma ruta explícita de apagado que la aplicación.

Interfaz comprobada en 1440×900 y 1280×800, incluyendo selección RGB/IR/webcam,
falta de Z y lista de cámaras. Compilación Release Linux: **PASS**.

La prueba real IR se intentó, pero otra instancia de Guipper tenía la Kinect
ocupada (`LIBUSB_ERROR_BUSY`); la webcam también estaba en uso. Solo llegaron
unos pocos cuadros IR, sin detecciones. **No se valida precisión, oscuridad ni
rendimiento a partir de ese intento.** Quedan pendientes los ciclos de pinza a
1,2–2 m, la reconexión física y los diez minutos con tres fuentes simultáneas.
La modalidad IR permanece experimental.

Los resultados siguientes corresponden a la implementación RGB anterior y no
validan esta ampliación.

## Validación local — 20 de octubre de 2026

Compilación Release Linux y pruebas de núcleo, historial, transiciones y audio:
**PASS**. Codec XML, copia, undo/redo, edición del vínculo, grupos, parámetros
expuestos, exclusión manual/MIDI/OSC, runtime ausente/bloqueado, cancelación y
reinicio: **PASS** en el ejecutable con perfil temporal. Regresión ANALOG COLOR:
**PASS**. UI inspeccionada a 1440×900 y 1280×800.

Sesión real interactiva de diez minutos, más treinta segundos sin seguimiento:

| Medida | Resultado |
| --- | --- |
| Resultados reales | 7.164 |
| Resultados con manos / con Z de palma | 4.251 / 4.240 |
| Resultados con dos manos | 1.356 |
| Resultados por segundo, incluyendo reconexiones | 12,16 |
| FPS de captura, mediana | 14,37 |
| Inferencia, p95 | 22,15 ms |
| Edad desde recepción del RGB-D, p95 | 45,21 ms |
| Render con / sin seguimiento, media | 60,01 / 60,00 FPS |

Se excluyeron 330 cuadros (10,92 s) del proveedor de simulación activado durante
la sesión. El capturador CPU y las pérdidas USB limitan la tasa: **no se alcanzó
el objetivo sostenido de 15 resultados/s**. La edad indicada no incluye exposición
ni decodificación y no demuestra por sí sola latencia física de extremo a extremo.
Las primeras partes de la sesión incluyeron compilación y regresiones paralelas;
los últimos tres minutos y la referencia sin seguimiento estuvieron libres de
esas tareas. La precisión gestual y la lateralidad aún requieren confirmación
visual del usuario.

Reconexión USB sin reiniciar Guipper: **confirmada por el usuario**, además de los
reinicios de dispositivo registrados en el log. Cierre con captura activa:
**PASS** en otra prueba real breve; se detuvieron las transferencias y se liberó
USB. El runtime queda instalado en el perfil habitual para usarlo normalmente.
Qbs se actualizó, pero no está instalado en este equipo para ejecutar esa ruta.
Windows/macOS permanecen fuera del alcance de esta entrega.

Resumen reproducible de medidas y condiciones:
[`kinect-hands-linux-2026-10-20.json`](../tests/results/kinect-hands-linux-2026-10-20.json).

## Validación de Cuerpo RGB — 21 de octubre de 2026

Compilación Release Linux, pruebas de señales corporales, confianza, proporciones,
vencimiento y políticas de pérdida: **PASS**. Los dos modelos instalados procesan
los formatos probados y cierran por EOF. La integración gráfica cubre inferencia
compartida entre parámetros, sesiones de manos/cuerpo independientes, XML,
copia, edición deshacible, grupos y parámetros expuestos. La selección visual,
el cambio inverso con clic derecho y la medición de cinco segundos con
undo/redo también pasaron. El panel se inspeccionó
con simulación y con la imagen real de la cámara.

Prueba breve con DJI Osmo Action 5 Pro: 90 segundos con tres parámetros y
30 segundos sin seguimiento, sin compilaciones concurrentes. Se obtuvieron
2.654 resultados (**29,49/s**, incluido inicio); inferencia p95 **13,03 ms**,
edad p95 **41,18 ms** y render mediano **60,00 FPS** tanto activo como detenido.
Se observaron señales válidas y variables de torso, separación de muñecas y
altura de brazos. No todos los cuadros tenían los puntos necesarios visibles:
esos intervalos no están anotados y sus recuentos no miden precisión.

Es una prueba corta, no una validación de diez minutos. Quedan pendientes esa
prueba prolongada, reconexión USB con cuerpo, lateralidad corporal de Kinect RGB
y la confirmación del usuario sobre respuesta al movimiento. En esa validación Cuerpo RGB todavía no se
ofrecía en infrarrojo; no se presenta como reconstrucción 3D. Medidas y condiciones:
[body-rgb-linux-2026-10-21.json](../tests/results/body-rgb-linux-2026-10-21.json).

## Validación de Público — 21 de octubre de 2026

Release Linux: **PASS**. Pruebas de zonas, centro/mediana, movimiento, ruido,
independencia del FPS, pérdidas, TTL, datos inválidos y aprendizaje de fondo:
**PASS**. El núcleo pasó AddressSanitizer y UndefinedBehaviorSanitizer;
LeakSanitizer no se ejecutó porque el entorno usa ptrace.

Integración en perfil temporal: **PASS** para profundidad sin RGB ni modelo,
varios parámetros con el mismo análisis, áreas independientes, reinicio sin
reutilizar fondo, XML, duplicación, delete/undo/redo, grupos y parámetros expuestos.
Se corrigió el redondeo de `ofXml::set(float)` en la configuración de seguimiento:
los números se guardan con precisión suficiente para recuperar el mismo float.
La prueba incluye extremos y valores de calibración con más de seis cifras.

UI inspeccionada a 1280×800, con regresión del panel de manos a 1440×900:
**PASS** para área arrastrable, undo/redo, aprendizaje, selección inversa de zonas,
calibración y convivencia con el inspector. Las etiquetas del área de captura
se ajustaron para evitar solapamientos. La prueba real se ejecuta por separado
para que compilaciones y regresiones no alteren las medidas.

Kinect real, rango 3–4,5 m, tres parámetros y análisis de profundidad compartido:
la primera prueba de 90 s reveló pérdidas de señal de movimiento por cadencia
irregular, aunque la profundidad seguía siendo válida. Se añadió una regresión
y una referencia temporal adaptable, sin aceptar resultados de más de 250 ms.

Confirmación de 60 s y 30 s sin seguimiento: **667 resultados**, de los cuales
**666** entregaron movimiento válido; el primero inicia la comparación temporal.
Análisis mediano **1,60 ms**, p95 **1,92 ms**; edad p95 **23,70 ms**; render mediano
**60,00 FPS** tanto activo como detenido. La cobertura mediana fue **78,8 %**.
Estos números describen continuidad y coste del cálculo, no precisión de
seguimiento de personas. La captura quedó en **11,12 resultados/s**, incluido
inicio, con pérdidas de paquetes informadas por libfreenect2: no se alcanzan
15 resultados/s sostenidos en esta comprobación.

La profundidad y la salida se inspeccionaron visualmente. El cierre de la
captura liberó USB. Quedan pendientes la confirmación del usuario sobre
oscuridad/movimiento, el aprendizaje real con pista vacía, varias personas en
el evento, reconexión física USB con este modo y la prueba prolongada de diez
minutos. Las pruebas de habitación no validan por sí solas el montaje de fiesta.
Registro: [audience-depth-linux-2026-10-21.json](../tests/results/audience-depth-linux-2026-10-21.json).

### Control de Público en vivo

Desde el vínculo, `Calibrar pista · 8 s` mide tres segundos de quietud y cinco
segundos de movimiento. Está disponible para Movimiento y ajusta solo ese
vínculo: mínimo al percentil 95 del ruido, máximo al percentil 90 del movimiento,
subida de 80 ms, bajada de 600 ms y zona muerta del 1 %. Requiere al menos
12 muestras de quietud y 20 de movimiento y un contraste mínimo de 0,005.
Si falta señal o contraste, conserva el ajuste anterior. No cambia el rango
físico de 3–4,5 m ni aprende el fondo automáticamente. Cancelar no aplica nada.

Los presets se recorren con clic izquierdo y se retrocede con clic derecho:

- **Energía:** movimiento de 0 a 0,15; subida 80 ms y bajada 600 ms.
- **Ocupación:** proporción de superficies aceptadas, de 0 a 1; subida 250 ms
  y bajada 1200 ms. No es un conteo de personas.
- **Desplazamiento:** centro horizontal de la actividad, con suavizado 250 ms.

Cada preset modifica la entrada del parámetro actual; conserva dispositivo,
ROI, zona, exclusión de fondo y límites de salida. Se puede deshacer. Subida y
bajada se aplican después de invertir/mapear la señal, con tiempos independientes
del FPS. Desactivar el botón `Subida / bajada` recupera el suavizado simétrico.
Los vínculos antiguos mantienen ese suavizado. Los tiempos y el modo se guardan
en XML versión 5; las muestras y el proceso de calibración no se guardan.

La previsualización identifica **I/C/D** y muestra movimiento de cada zona con
barras doradas (escala visual 0–0,15). Fuera de `Área y fondo`, pulsar una zona
la selecciona. En `Área y fondo`, arrastrar sigue delimitando la ROI.
`Recibiendo bien` indica profundidad reciente y cobertura ≥50 %;
`Poca profundidad válida` indica cobertura insuficiente; `Captura interrumpida`
indica un resultado de más de 250 ms. Estos estados no certifican precisión.
La entrada, señal mapeada y salida efectiva siguen visibles debajo del editor.

La captura de Público tiene ahora una suscripción de profundidad independiente:
no genera RGB ni normaliza IR para manos si no hay consumidores de manos IR.
Comparte la apertura USB con el resto de fuentes Kinect. Esto reduce trabajo
CPU; no garantiza recuperar paquetes USB perdidos. No se cambian ajustes del
sistema ni se reinicia la cámara silenciosamente.

Para repetir la medición de diez minutos (perfil y composición temporales):

```sh
GUIPPER_TRACK_CROWD=1 GUIPPER_HAND_BENCH_SECONDS=600 python3 tests/hand_graphics_test.py --real
python3 tests/audience_benchmark_report.py /tmp/guipper-hands-XXXXXXXX/data/hands-benchmark.csv
```

El informe separa FPS de render activo/detenido, análisis, edad, huecos de más de
250 ms y cuadros saltados por el consumidor. Los descartes del consumidor no son
los paquetes USB que registra libfreenect2. La edad empieza al recibir el cuadro
decodificado; no incluye la exposición del sensor ni la ventana de movimiento.

Diagnóstico local de la captura: la Kinect negocia 5000 Mbit/s y su control de
energía USB está en `on`. El driver instalado usa `CpuPacketPipeline`; durante
la medición registra unos 68 ms por decodificación de profundidad, frente a
aproximadamente 1,6 ms para Público. La instalación actual no incluye OpenCL ni
CUDA. No se activó OpenGL de captura desde el worker: requeriría resolver y
validar por separado su contexto gráfico y ciclo de vida. Las pérdidas reportadas
por el parser no permiten atribuir por sí solas el problema al cable USB.

Validación de controles en vivo (2026-10-21): Release Linux, núcleo de Público,
manos/cuerpo, historial, persistencia y panel nativo **PASS**. Se verificó el
preset al deshacer: su nombre se obtiene de la configuración efectiva, no del
último botón pulsado. Los ajustes que no coinciden muestran `personalizado`.

Prueba real completada: **600 segundos activos + 30 detenidos**, tres parámetros,
**7243 resultados**, **7242 con movimiento válido** (el primero inicializa),
cero huecos de más de 250 ms y cero cuadros saltados por el consumidor. Promedio
**12,07 resultados/s**; análisis mediano **1,57 ms**, p95 **1,91 ms**; edad p95
**18,67 ms** desde recepción decodificada. Render mediano **60,00 FPS** activo
y detenido. USB liberado al detener. El driver continúa informando pérdidas de
paquetes y no se alcanzó el objetivo de 15 resultados/s. Esta prueba comprueba
estabilidad en el equipo, no la respuesta y calibración con público real. Público
usa infrarrojo activo para medir profundidad y no necesita luz visible.

Resultado completo: [audience-live-controls-linux-2026-10-21.json](../tests/results/audience-live-controls-linux-2026-10-21.json).

## Cuerpo con Kinect infrarrojo — 23 de octubre de 2026

Esta sección registra la primera validación con una persona. La ampliación posterior a dos personas se describe en el apartado siguiente.

En el vínculo de un parámetro o en el panel de seguimiento, elegir **Kinect infrarrojo → Cuerpo IR → Iniciar esta fuente**. Entrega las mismas señales que Cuerpo RGB: torso X/Y, separación de muñecas, altura de brazos y los 33 puntos X/Y de una persona. No añade distancia corporal en metros ni seguimiento de varias personas.

Usa el cuadro IR normalizado de 512×424, convertido a tres canales para el Pose Landmarker Lite existente. La previsualización y el esqueleto conservan el aspecto de ese cuadro. No espera un cuadro RGB, no sustituye IR por RGB y comparte la apertura USB con los demás consumidores Kinect. Cuerpo IR, Cuerpo RGB, Manos IR y Público tienen identidades de sesión independientes. Pasar de Público a Cuerpo conserva la selección IR.

Se conserva el vencimiento de 250 ms, los umbrales de confianza, las opciones de pérdida y el mapeo habitual. XML guarda `source=kinect_ir` y `detector=body`, sin cambiar los identificadores de automatización. No requiere otro modelo ni instalar un runtime diferente si ya funciona Cuerpo RGB.

**Experimental:** ejecutar Pose Landmarker sobre IR no demuestra precisión en oscuridad. Falta validar articulaciones, oclusiones, lateralidad y pérdidas con una persona real, particularmente a 3–4,5 m. Público por profundidad sigue siendo la opción existente para medir movimiento/ocupación sin depender de un esqueleto.

Validación local: compilación Release, `tracking_body_tests`, `hand_tracking_tests` y harness gráfico de persistencia aprobados. La simulación comprueba formato IR 512×424, vínculo/XML y aislamiento al detener Cuerpo IR mientras sigue Cuerpo RGB. Prueba real de 30 segundos con tres parámetros vinculados: 266 muestras con cuadros IR, 130 con cuerpo presente; captura mediana 9,97 FPS, inferencia 9,98 FPS, tiempo de inferencia p95 19,43 ms, edad del resultado p95 36,72 ms y render mediano 60 FPS. La edad mide el timestamp publicado por captura, no latencia física de extremo a extremo. El log registra pérdidas de paquetes de profundidad; no se alcanzaron 15 resultados/s. Cerró correctamente con captura activa. Artefactos temporales: `/tmp/guipper-hands-t57fzb2q`.

La proporción de muestras con cuerpo no mide precisión: no hubo anotación de presencia, distancia, iluminación ni movimientos. La captura de diagnóstico confirma imagen IR y overlay, pero quedan pendientes exactitud del esqueleto, falsos positivos, lateralidad, reconexión y prueba prolongada en las condiciones de la fiesta. Las pruebas usaron una copia temporal de los datos, sin modificar composiciones ni respaldos.

## Dos personas por fuente y selección de players — 23 de octubre de 2026

Cuerpo RGB y Cuerpo IR ahora solicitan **hasta dos poses** al detector, en una sola inferencia por fuente. Cuerpo IR conserva su carácter experimental. Los vínculos existentes se cargan con **Auto: más centrada**; no se inicia captura al cargar. Cada vínculo permite elegir:

- **Auto: más centrada:** el torso más próximo al centro horizontal de la imagen. Puede cambiar de persona deliberadamente según su posición.
- **Auto: más cercana:** menor distancia medida en el interior del torso con profundidad Kinect. Si falta profundidad válida no inventa una distancia ni utiliza el Z del modelo. No se ofrece al recorrer opciones en webcam; una configuración cargada se conserva y muestra la limitación.
- **Persona 1 / Persona 2:** conservan el número durante la sesión y no pasan a la otra persona cuando falta la elegida. El tratamiento del valor al perder señal sigue la opción del vínculo.

El diagnóstico dibuja ambos esqueletos en cian y oro, con etiquetas **P1/P2** y distancia si está disponible. Clic en la etiqueta selecciona la persona sin cambiar la señal; clic en una articulación selecciona esa persona y ese punto. El selector admite clic izquierdo para avanzar y derecho para retroceder. La elección se guarda por parámetro (`kinect/person`, versión 6; `movtype=7` intacto), incluida copia e historial. Los números son locales a cada fuente/sesión: P1 de RGB no identifica necesariamente a P1 de IR. No son los IDs del SDK oficial de Kinect ni reconocimiento de identidad personal.

La asociación usa torso, desplazamiento reciente y profundidad cuando existe; no usa el orden de las poses que devuelve Python. Inicialmente numera de izquierda a derecha las personas detectadas juntas. Conserva posiciones durante pérdidas de hasta dos segundos. Un cruce sin asociación clara suspende las asignaciones. Tras dos segundos sin una asociación válida, libera los lugares y vuelve a detectar automáticamente, reiniciando el historial de movimiento. No garantiza conservar el número de una persona que sale y vuelve. **Reasignar personas** reinicia solo el detector de esa fuente y vuelve a asignar números para todos sus vínculos. No reinicia USB ni afecta otras fuentes. Si no quedan vínculos activos ni diagnóstico abierto, la inferencia corporal se detiene; hay que iniciarla de nuevo para asignar identidades, evitando una reasignación automática oculta al restaurar objetos.

El runtime mantiene el protocolo de cuadros RGB8 existente y añade capacidad `max_bodies=2` y resultados `bodies` (máximo dos arrays de 33 puntos). Un runtime antiguo se rechaza con una indicación de actualización. `scripts/install_hand_tracking_linux.sh` instala el worker actualizado usando el mismo modelo fijado; en este equipo se actualizó únicamente `worker.py`, sin descargar modelos.

Limitaciones: la asociación es geométrica y conservadora, no garantiza identidad ante oclusiones prolongadas o movimientos muy rápidos. Las opciones automáticas pueden cambiar de sujeto; para control individual usar Persona 1/2. Dos poses configuradas no demuestran que se detecten correctamente dos personas en IR, especialmente con público superpuesto a 3–4,5 m. La validación real de ese escenario y de su rendimiento queda separada de las pruebas simuladas.

Validación de esta ampliación: Release compilado; pruebas de señales/asociación, regresión de manos, modelos/protocolo instalado, persistencia y UI aprobadas. El harness verificó dos personas simuladas controlando distintos vínculos, arrays de dos cuerpos a través de un proceso real de prueba, rechazo de runtime antiguo, cierre sin consumidores, selección por punto, recorrido inverso del selector, compatibilidad XML, copia y delete/undo/redo. La revisión visual final confirmó el espacio entre controles y la selección visible en el resumen del parámetro.

La prueba real de RGB+IR simultáneos de 30 s **no valida captura ni rendimiento**: otra instancia de Guipper tenía abierta la Kinect (`LIBUSB_ERROR_BUSY`). Se recibieron solo tres resultados IR, sin cuerpos, y ninguno RGB; no son una muestra útil para medir precisión o FPS. Se dejó esa instancia abierta. Quedan pendientes dos personas reales, cruces/oclusiones, oscuridad a 3–4,5 m, reconexión y medición prolongada. No se modificaron las composiciones ni los respaldos existentes. Detalle: [body-multiperson-linux-2026-10-23.json](../tests/results/body-multiperson-linux-2026-10-23.json).

## Energía y desplazamiento corporal para controlar efectos

Además de posiciones y gestos, Cuerpo RGB/IR ofrece cinco señales continuas:

- **Energía corporal:** velocidad media de hombros, codos, muñecas, caderas, rodillas y tobillos válidos, relativa al ancho de hombros. Detecta tanto desplazamiento general como movimiento de extremidades; no mide emociones ni demuestra que el público disfrute. Entrada inicial 0–1,5 anchos de hombros/s, ajustable.
- **Desplazamiento X/s / Y/s:** desplazamiento medio de puntos válidos del torso por segundo, en fracciones de la imagen. X positivo hacia la derecha; Y positivo hacia arriba. Rango inicial −0,5 a +0,5; quietud en cero. No es velocidad física en metros.
- **Centro de actividad X / Y:** posición de las articulaciones que se mueven, ponderada por su actividad, entre 0 y 1. Se señala con una cruz blanca en la imagen. Sin movimiento no entrega una posición inventada.

El selector de personas añade **Promedio del grupo**: usa las personas rastreadas con señal válida, como máximo dos. La energía promedia magnitudes; las velocidades promedian dirección (movimientos opuestos pueden cancelarse). El centro de actividad del grupo pondera cada persona por su energía. Se puede limitar a **toda la imagen / izquierda / centro / derecha**, según dónde cae el centro del torso. Las regiones son tercios horizontales de la imagen sin espejo, y la seleccionada se resalta. Un punto individual usa su selección de articulación y no aplica el filtro de zona.

El cálculo conserva un historial acotado por persona y compara con una posición interpolada 150 ms antes; no depende de la frecuencia de render. Rechaza puntos sin confianza, cambios fuertes de escala/aspecto y saltos de pose; descuenta un margen pequeño de ruido relativo al ancho de hombros. Tras pérdida, cambio de sesión o huecos de más de 250 ms vuelve a reunir historial, sin derivar velocidades entre identidades o cuadros separados por la pérdida. Es una estimación visual: su ruido y sensibilidad necesitan calibración con la cámara y distancia reales.

**Ejemplo de intensidad:** abrir el vínculo del parámetro `mix` de un efecto, elegir **Cuerpo IR → Energía corporal → Promedio del grupo → toda la imagen**, e iniciar. Ajustar la entrada con **Medir mi recorrido · 5 s**, alternando quietud y movimiento; mantener la salida propia del parámetro, por ejemplo 0–1. Energía activa por defecto la envolvente con subida de 100 ms y bajada de 800 ms; se pueden ajustar o cambiar a suavizado simple. Para regresar al aspecto base cuando no haya personas, elegir **Al perder: volver suavemente al valor base**. La composición no se altera automáticamente.

Para una pista oscura con más gente a 3–4,5 m, usar el modo existente **Público → preset Energía → Calibrar pista** y ajustar el área/distancias. Trabaja con profundidad y no depende de obtener esqueletos. Sus señales Movimiento/Actividad X/Y también pueden alimentar intensidad o posición. El promedio de dos esqueletos no representa a toda una multitud; IR corporal sigue experimental. Estas señales automatizan parámetros, no cambian automáticamente de caja, shader o composición.

Persistencia: se añaden señales al final de la enumeración y `Promedio del grupo` al final de la selección de personas. XML de seguimiento versión 7 guarda `bodyzone`, señal, grupo y envolvente sin guardar historial de movimiento. Los archivos anteriores mantienen los mismos identificadores y zona completa por defecto.

La interfaz conserva las filas y botones nativos existentes: cian para estados activos, dorado para la zona y el aviso experimental, blanco para el centro de actividad.

Validación local de movimiento: Release, pruebas de cálculo, regresión de manos, persistencia y UI aprobadas. Se verificaron traslación constante a 15/30/60 FPS, quietud con ruido pequeño, pérdidas, historial, confianza, promedio del grupo, zonas vacías, selección inversa y envolvente. El harness usó un perfil temporal y dos personas simuladas; no valida precisión ni fluidez con público real. Quedan pendientes calibración en pista, oscuridad a 3–4,5 m, oclusiones y medición prolongada. Detalle: [body-motion-linux-2026-10-23.json](../tests/results/body-motion-linux-2026-10-23.json).


## Promedio y exclusión de personas

Los vínculos nuevos usan **Promedio** por defecto; los ya guardados conservan su selección. Elegir Promedio en el selector de personas para combinar P1 y P2. Se limita cada contribución al rango de entrada antes de promediar: brazos abajo (0 %) y arriba (100 %) producen 50 %. Solo participan señales válidas de personas incluidas y dentro de la zona. Si falta una, la otra aporta el resultado completo; si no queda ninguna, rige la política de pérdida. El centro de actividad conserva su ponderación por energía.

**P1 activo/excluido** y **P2 activo/excluido** permiten anular y reactivar cada lugar por parámetro, incluso cuando no está visible. No detienen la captura ni cambian otros vínculos. XML versión 8 guarda la máscara `players` (ambos incluidos por defecto en archivos antiguos), con copia y undo/redo. La exclusión corresponde al lugar, no a una identidad biométrica: tras una ausencia prolongada puede ocuparlo otra persona.

La interfaz conserva botones nativos, cian para incluidos y texto explícito para excluidos. Máximo actual: dos poses por fuente RGB o IR; Público sigue siendo la opción de movimiento global sin identificar personas. La recuperación y mezcla se prueban automáticamente; precisión y recuperación con personas reales quedan pendientes de comprobar con la Kinect.

Validación de promedio y exclusión: Release, pruebas de cálculo/manos, persistencia y prueba gráfica aprobadas; revisión visual sin recortes ni superposiciones. La prueba verifica 0 % + 100 % = 50 %, exclusión/reactivación, ambos excluidos y recuperación tras ausencia prolongada/cruce en simulación. Resultados: [body-players-linux-2026-10-23.json](../tests/results/body-players-linux-2026-10-23.json).


## Cuatro personas por fuente

Cuerpo RGB e IR ahora solicitan **hasta cuatro poses** al único worker de cada fuente. El número es una capacidad configurada, no una garantía de detección: IR sigue experimental, especialmente a 3–4,5 m con oclusiones. Manos mantiene su límite de dos manos; Público no cambia.

El selector permite Persona 1–4 y Promedio. Los cuatro botones **P1–P4 activo/fuera** controlan la inclusión por parámetro; el tooltip explica su alcance. La previsualización usa cuatro colores y etiquetas compactas cuando hay más de dos personas. Los vínculos nuevos incluyen las cuatro; los antiguos conservan su máscara y, si no la tenían, incluyen P1/P2. Activar P3/P4 explícitamente para ampliar un vínculo anterior. Los números son lugares locales, no identidades biométricas ni IDs compartidos entre RGB e IR.

La asociación compara globalmente las asignaciones de hasta cuatro poses, conserva reservas de dos segundos y rechaza alternativas ambiguas. La inferencia sigue compartida entre parámetros; cada jugador tiene su historial de movimiento. P3/P4 se añaden al final de la enumeración sin cambiar Group=4 ni Persona 1/2. XML versión 9 admite la máscara de cuatro bits y selección individual de P3/P4, sin guardar estado de seguimiento.

El runtime instalado debe actualizarse con `scripts/install_hand_tracking_linux.sh`; un worker antiguo genera la indicación de actualizar en lugar de simular soporte para cuatro. No se agregaron dependencias ni modelos. La simulación del diagnóstico muestra cuatro personas y permite verificar selección, exclusión y reactivación sin cámara.

Validación de cuatro personas: Release, pruebas de asociación/mapeo, runtime instalado, transporte de cuatro poses, XML de P4/máscara, undo/redo e interfaz aprobados. La prueba visual usa cuatro esqueletos simulados; no acredita rendimiento ni precisión con cuatro personas reales. Quedan pendientes oscuridad a 3–4,5 m, oclusiones y métricas de latencia/FPS. Resultados: [body-four-players-linux-2026-10-23.json](../tests/results/body-four-players-linux-2026-10-23.json).


## Cinco efectos de imagen para el público

En `shaders/imageprocessing`, buscar `audience_`. Conectar una imagen, video, cámara o salida de otra caja a `textura1`. Todos exponen `amount` (0–1): cero conserva la imagen original. Se pueden encadenar. No requieren Kinect para funcionar; cualquier parámetro admite los modos habituales de automatización.

| Shader | Efecto | Vínculo corporal sugerido |
| --- | --- | --- |
| `audience_waves` | Ondas radiales desde un centro desplazable | Altura brazos → amount; Torso X/Y → center_x/center_y |
| `audience_vortex` | Remolino localizado | Altura brazos → amount; Desplazamiento X/s → twist |
| `audience_rgb_split` | Separación de canales RGB direccional o radial | Energía corporal → amount; Torso X → direction |
| `audience_neon` | Contornos coloreados sobre la imagen | Energía corporal → amount; Altura brazos → hue |
| `audience_mosaic` | Bloques cuadrados, reducción de colores y trama | Altura brazos → block_size; Energía corporal → amount |

Para combinar personas: abrir el vínculo del parámetro, elegir Cuerpo IR o RGB, seleccionar la señal y **Promedio**, incluir P1–P4 e iniciar la fuente. También se pueden repartir controles entre personas, por ejemplo P1 → centro X, P2 → centro Y, P3 → intensidad y P4 → color. X crece hacia la derecha, Y hacia arriba; la imagen no se espeja automáticamente.

Comenzar con entrada calibrada, suavizado de 150–300 ms y salida 0–1. Para intensidad basada en movimiento, usar la envolvente de Energía corporal. Para Altura brazos, usar el recorrido propio de cada instalación; la señal inicial va de 0 a 2. `twist=0.5` es neutro; por debajo gira hacia un lado y por encima hacia el otro. En mosaico, `color_steps` alto conserva más colores. `speed=0` detiene la animación propia de las ondas.

Para una pista oscura donde los esqueletos no se detectan de forma estable, vincular `amount` a **Público → Movimiento** con calibración de pista. Los shaders no mejoran ni validan la precisión de Cuerpo IR. No incluyen estrobo automático ni usan feedback acumulativo. Conservan el alfa de la fuente; las deformaciones y el mosaico desplazan también su transparencia.

Validación inicial de los seis shaders (kaleido retirado posteriormente a pedido del usuario): compilación/render nativos, neutralidad RGBA con amount=0, alfa constante, controles extremos y formatos horizontal/vertical aprobados. Patrón de prueba sintético; sin medición de rendimiento a resolución completa ni prueba con público real. Resultados: [audience-shaders-linux-2026-10-23.json](../tests/results/audience-shaders-linux-2026-10-23.json).


## Dona 3D con centro: SOUL TORUS

`shaders/generative/soul_torus.frag` genera una dona tridimensional reflectante y una esfera central, inspiradas en el video de referencia. No necesita entrada de imagen. El texto se añade con TEXTO por separado. El material usa iluminación de estudio procedural sobre fondo negro; `transparent` permite componerlo sobre otra imagen.

Todos los controles numéricos están normalizados a 0–1: `zoom`, `thickness` (grosor), `center_size` (tamaño central, cero lo oculta), `tilt_x`/`tilt_y` (inclinación, 0.5 neutro), `rotation`, `speed` (cero detiene el giro), `deform` (ondulación leve), `light_angle`, `brightness`, `rim`, `hue` y `color_amount` (cero deja el material gris). La esfera central se limita al espacio interior para evitar atravesar el aro.

Vínculos sugeridos: Promedio de altura de brazos → `thickness`; energía corporal → `deform` o `brightness`; Torso X → `tilt_y` con salida acotada, por ejemplo 0.4–0.65. También se pueden asignar P1–P4 a controles independientes. Para inclinación manual o corporal sin giro automático, fijar `speed=0`. La geometría se dibuja con ray marching; medir el rendimiento a la resolución real antes de acumular muchas instancias.

SOUL TORUS validado con compilación y render nativos a 640×360 y 240×320, valores iniciales y extremos, y fondo opaco/transparente. Revisada la imagen inicial; rendimiento a resolución completa pendiente. Resultados: [soul-torus-linux-2026-10-23.json](../tests/results/soul-torus-linux-2026-10-23.json).

Ajuste de SOUL TORUS según la referencia ampliada: cuerpo oscuro, reflejo localizado arriba a la derecha y halo en el borde interior/exterior. Los valores iniciales ahora son tilt_x=0,5, tilt_y=0,52, light_angle=0,5 y speed=0. Los vínculos/composiciones existentes conservan sus valores; ajustar esos controles para reproducir la posición de referencia. El shader activo del equipo fue actualizado sin sobrescribir ediciones distintas.
