# Kinect: manos y parámetros (Linux)

Kinect v2, una persona, hasta dos manos. El seguimiento se realiza localmente con
MediaPipe Hand Landmarker. No necesita una caja Kinect en el grafo. No contiene
seguimiento corporal ni reconstrucción 3D.

## Instalación

Necesita libfreenect2 (ver `scripts/install_kinect2_linux.sh`), USB 3 y alimentación
de Kinect. El runtime usa Python 3.12 o posterior, `python3-pip`, soporte `venv`,
`curl` y `sha256sum`.

```sh
./scripts/install_hand_tracking_linux.sh
make Release JOBS=-j2 -j2
```

El instalador crea un entorno independiente en
`$XDG_DATA_HOME/Guipper/hand-tracking` (por defecto
`~/.local/share/Guipper/hand-tracking`). `GUIPPER_HAND_RUNTIME` permite cambiarlo.
No utiliza el runtime de seguimiento corporal anterior. Las versiones están en
`scripts/hand_tracking/requirements.lock`; el modelo Hand Landmarker float16/v1
se verifica con SHA-256. No se descarga nada al abrir una composición.

## Uso

1. En un parámetro numérico, pulsar **K**. También se puede abrir el diagnóstico
   desde **SETTINGS → Kinect**.
2. Elegir mano y señal. Los botones recorren las opciones. Para posición X/Y,
   seleccionar palma o un punto de 0 a 20. La imagen no está espejada: X crece
   hacia la derecha y Y hacia arriba.
3. Ajustar los extremos de entrada, o capturarlos con la mano en cada extremo.
   La salida usa el rango del parámetro, incluidos sus límites personalizados.
4. Pulsar **Activar vínculo** e **Iniciar seguimiento**. Son acciones separadas:
   una composición nunca abre la cámara automáticamente por contener vínculos.

Ejemplo: ANALOG COLOR → pixel size → K → mano derecha → Pinza. Configurar la
salida del parámetro de 1 a 32; entrada inicial 0,15 a 1. Cerrar la pinza reduce
el tamaño de bloque. Se puede vincular la misma señal a varios parámetros.

**Simulación** inicia un proveedor sintético, identificado en el panel. **Perder**
y **Volver** reproducen pérdidas. No es una prueba de la cámara ni del modelo.
El panel se puede arrastrar por su cabecera y ensanchar desde el borde derecho;
convive con el inspector. El gráfico conserva las últimas 240 muestras y cada
curva usa su rango: cruda (ámbar), normalizada antes del suavizado (cian), final (blanco).

Al perder una mano o superar 250 ms desde la captura, se mantiene el último valor.
No se borra la mano elegida ni el vínculo. Un Z inválido solo invalida Distancia.
Reiniciar/detener la sesión restablece el valor base; desactivar el vínculo deja
el valor actual bajo control manual. MIDI/OSC no escriben valores mientras
Kinect sea el modo activo. Audio y patrones siguen siendo modos excluyentes.

## Señales y diagnóstico

- Pinza: distancia pulgar (4)–índice (8) / ancho de palma (5–17), con corrección
  de aspecto RGB. Rango inicial 0,15–1.
- Apertura: media de la extensión angular normalizada de los cuatro dedos largos.
- Posición: palma (centro de muñeca y cuatro nudillos), o cualquiera de los 21 puntos.
- Distancia: mediana de un parche interior de palma de 11×11 píxeles del color
  original, registrado mediante libfreenect2. Se exigen al menos 9 muestras
  válidas de 0,5–4,5 m y dispersión interdecílica menor o igual a 100 mm.
  No utiliza el Z del modelo. Entrada inicial 0,7–1,5 m.

El RGB se reduce a 960×540. El parche de profundidad usa 1920×1080, descartando
las filas extra superior e inferior de `bigdepth` (1920×1082). Color y profundidad
se publican juntos con número de cuadro y reloj monotónico. La textura se carga
solo en el hilo principal. Captura USB, registro, transporte e inferencia quedan
fuera de ese hilo. La edad se mide desde la recepción del par RGB-D decodificado,
antes del registro; no incluye la exposición del sensor ni su decodificación
USB. No equivale a una medición física de movimiento a pantalla.

Hay un proceso Python y una única captura compartida. Cada inferencia toma el
cuadro más reciente; los parámetros leen el mismo resultado por actualización.
Los cruces cercanos, coincidencias espaciales ambiguas y cambios contradictorios
de lateralidad se rechazan en vez de intercambiar controles. Esto puede producir
una pérdida breve. No es identificación persistente de personas.

El panel muestra captura/inferencia FPS, milisegundos de inferencia, edad total,
descartes de la cola del detector, puntos, lateralidad y estado de Z.
Los paquetes USB perdidos que informa libfreenect2 se registran aparte en el log. El estado diferencia runtime ausente,
cámara ausente, inicio, búsqueda, seguimiento, vencimiento, detención y error.
**Reconectar** invalida la sesión previa y reinicia el detector y la captura.
**Detener** libera la participación del detector; otra caja Kinect puede seguir
utilizando la cámara. Si no hay panel ni parámetros actualizándose, la inferencia
se suspende y libera su participación en la captura.

## Persistencia y pruebas

`movtype=7` se añadió al final. El codec XML guarda configuración y valor base,
no imágenes, IDs de manos, sesiones ni valores filtrados. Copia y undo reinician
el filtro. Los vínculos sobreviven dentro de grupos y parámetros expuestos.
Sin runtime/cámara, la composición carga y conserva su base. Windows y macOS
pueden cargar la configuración; esta primera implementación no captura allí.

```sh
g++ -std=c++17 tests/hand_tracking_tests.cpp -o /tmp/hand-tests
/tmp/hand-tests
GUIPPER_HAND_RUNTIME=/ruta/al/runtime python3 tests/hand_worker_test.py
python3 tests/hand_graphics_test.py
python3 tests/hand_graphics_test.py --real
```

Las pruebas gráficas crean copias en `/tmp`, con perfil separado; no escriben
las composiciones ni respaldos del usuario. El modo real controla tres parámetros
y registra diez minutos con seguimiento más treinta segundos de referencia sin
seguimiento, en `hands-benchmark.csv`. Solo guarda métricas, sin imágenes de la
cámara. Para una comprobación breve, `GUIPPER_HAND_BENCH_SECONDS=30` reduce la
fase activa; no sustituye la medición de diez minutos.
`GUIPPER_HAND_CLOSE_ACTIVE=1` termina al final de esa fase con la captura aún
activa para comprobar el cierre de aplicación.

Criterio de aceptación real: pinza, X/Y, profundidad, inmovilidad, pérdida/regreso,
cruces y reconexión USB; al menos 15 resultados/s y edad p95 inferior a 200 ms con
iluminación adecuada. Medir aparte captura y render. Compilar o pasar simulación
no demuestra precisión ni fluidez con Kinect. Ver resultados de esta entrega al
final del documento.

Referencias: [Hand Landmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker/python),
[registro libfreenect2](https://openkinect.github.io/libfreenect2/classlibfreenect2_1_1Registration.html).

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
