# iPhone LiDAR (Record3D por USB)

Un iPhone o iPad con LiDAR funciona como segundo sensor de profundidad métrica,
al lado de la Kinect v2. La app de iOS **Record3D** transmite profundidad,
confianza y color por el cable USB; Guipper los lee directamente.

Usos:

- **Caja IPHONE LIDAR** (`Shift+L`): salida COLOR, DEPTH o CONFIDENCE, con la
  misma convención de grises que la caja Kinect (cerca = brillante). Sirve como
  entrada de cualquier shader de desplazamiento.
- **PointerCloud**: el selector **KINECT V2 / IPHONE LIDAR** en el inspector
  elige el dispositivo. Se guarda en la sesión. **detail** (1–4) agrega puntos
  entre los píxeles del sensor, interpolados solo dentro de una misma
  superficie; con `density` en 1 la nube del iPhone queda continua.
  **smooth** y **fill holes** limpian la profundidad (también la de la Kinect).
- **Público**: en el panel de seguimiento, con **Publico** elegido, el tercer
  botón de fuente es **iPhone LiDAR**. Movimiento, ocupación, centros y
  distancia funcionan igual que con la Kinect.

Manos y Cuerpo no usan el iPhone: la fuente solo aporta profundidad.

## Requisitos

- Record3D con la compra de streaming en vivo, versión 1.10 o posterior.
- Linux. En Windows y macOS la caja existe pero muestra `NOT BUILT` (por ahora).

## Instalación (una vez)

```sh
./scripts/install_iphone_lidar_linux.sh
make Release JOBS=-j2 -j2
```

Instala `usbmuxd` y las bibliotecas de desarrollo de `libusbmuxd` y `liblzfse`
de la distribución. `config.make` activa `IPHONE_LIDAR` cuando las encuentra;
sin ellas Guipper compila igual y la caja indica qué falta.

## Puesta en marcha

1. Conectar el iPhone por cable, desbloquearlo y aceptar **Confiar en esta
   computadora**. `idevice_id -l` debe listarlo.
2. En Record3D: **Settings → USB Streaming mode** activado; pulsar grabar.
3. En Guipper: `Shift+L`, o elegir el dispositivo en PointerCloud o en Público.

El estado se muestra en la caja y en el inspector:

| Estado | Qué hacer |
| --- | --- |
| `SEARCHING IPHONE` / `NO IPHONE / USBMUXD NOT RUNNING` | Conectar el cable. `usbmuxd` arranca al detectar el teléfono. |
| `IPHONE NOT FOUND / TRUST THIS COMPUTER` | Desbloquear y aceptar la confianza; `idevicepair pair`. |
| `OPEN RECORD3D / ENABLE USB STREAMING` | Abrir la app y activar el modo USB. |
| `PRESS RECORD IN RECORD3D` | Conectado; falta pulsar grabar. |
| `PAUSED: PRESS RECORD IN RECORD3D` | La app dejó de enviar (grabación en pausa o pantalla bloqueada). |
| `STREAMING` | Funcionando. |
| `STREAMING (TRUEDEPTH, NOT LIDAR)` | Record3D usa la cámara frontal; cambiar a la trasera. |

El icono de cámara del inspector reconecta. Las cajas, PointerCloud y Público
comparten una sola conexión.

## Datos y límites

- Profundidad 256×192 (la Kinect da 512×424), en milímetros, con intrínsecos
  calculados a partir de la imagen RGB que envía la app.
- Rango útil aproximado de 0,3 a 5 m; desde unos 4 m aumenta el ruido. El rango
  de Público (3–4,5 m) queda en el borde: validar en el lugar.
- Los píxeles de confianza **baja** se descartan antes de publicar la
  profundidad: son los bordes falsos entre figura y fondo. **CONFIDENCE** muestra
  baja en negro, media en gris y alta en blanco.
- El LiDAR es un sensor activo y funciona sin luz, igual que la Kinect.
- El color solo se decodifica mientras alguna caja muestra COLOR.
- **smooth** (0–1) suaviza en el tiempo solo los cambios chicos, que son ruido;
  un salto grande (alguien que se mueve, un borde) pasa al instante, así que no
  deja estela. **fill holes** mantiene unos cuadros el último valor de un punto
  que se pierde y rellena huecos rodeados por una misma superficie; los huecos
  en un borde quedan vacíos.
- **16 bit depth** reparte la profundidad en dos canales: rojo es el gris de
  siempre y verde la fracción entre dos grises (65.536 niveles en vez de 256).
  Los shaders que leen solo rojo ven lo mismo; los `depth_` aprovechan los dos y
  sus curvas y relieves pierden los escalones. La miniatura se ve verdosa.
- La imagen sale en la orientación del sensor. **rotate 90** la gira un cuarto
  de vuelta; con `mirror` y `flip vertical` cubre cualquier montaje. En
  PointerCloud se usa `rotate Z`.

## Efectos con profundidad

Tres shaders en `imageprocessing/`, pensados para la salida DEPTH (sirven igual
con la Kinect):

| Shader | Entradas | Qué hace |
| --- | --- | --- |
| `depth_topo` | `depth` | Curvas de nivel de neón que envuelven el cuerpo y lo atraviesan. |
| `depth_echo` | `depth` | Siluetas de lo que está cerca, en ecos que se van por un túnel. |
| `depth_parallax` | `color`, `depth` | La imagen orbita en 3D: cerca y fondo se mueven en sentidos opuestos. |

Para `depth_parallax` usar dos cajas IPHONE LIDAR (una COLOR, otra DEPTH), las
dos con `stretch`; comparten la conexión. Con **16 bit depth** activado en la
caja DEPTH los tres se ven más suaves.

## Pruebas

```sh
make -C tests record3d_protocol_tests && tests/record3d_protocol_tests
python3 tests/iphone_lidar_test.py
```

`iphone_lidar_test.py` no necesita un iPhone: `tests/record3d_fake_device.py`
simula `usbmuxd` y Record3D en un socket propio (`USBMUXD_SOCKET_ADDRESS`), con
una figura que camina frente a una pared. Comprueba la profundidad en milímetros,
el descarte por confianza, los intrínsecos, COLOR/CONFIDENCE, PointerCloud,
Público, guardado/recarga, borrar/deshacer y el aviso de pausa. El simulador
también sirve para probar una sesión a mano:

```sh
python3 tests/record3d_fake_device.py /tmp/fake-usbmuxd.sock &
USBMUXD_SOCKET_ADDRESS=UNIX:/tmp/fake-usbmuxd.sock bin/Guipper
```

Pendiente: validar con el iPhone 15 Pro Max real (latencia, FPS, ruido a 3–5 m
con público).
