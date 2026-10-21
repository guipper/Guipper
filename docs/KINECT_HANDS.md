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
y la confirmación del usuario sobre respuesta al movimiento. Cuerpo RGB no se
ofrece en infrarrojo ni se presenta como reconstrucción 3D. Medidas y condiciones:
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
