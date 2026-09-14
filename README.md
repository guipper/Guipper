
# Guipper Real-time Visual Software

Documentation reviewed against the local source on 2026-09-12. See the
[technical handbook](mds/SKILLS.md), [architecture boundaries](mds/ARQUITECTURA.md)
and [current backlog](mds/FEATURE_BACKLOG.md).


<img src="https://github.com/guipper/Guipper/blob/main/muchosnodos.png" width="800" height="450">

<a href="https://mmtt.com.ar/guipper/"> WEBSITE </a><br>
<a href="https://www.youtube.com/watch?v=rOPMYAHRxRU&list=PLfar0iGmq1QLaV4g0J6rfsSUxda1kYTBn&pp=gAQBiAQB"> Video tutorials (Spanish only) </a>





## Brief Project Description and Purpose
Guipper is a software developed for real-time visual creation using GLSL fragment shaders. It allows for the execution of parameterized and generative visuals with extremely low overhead, while also providing the ability to add new shaders.

Special thanks to Kali Shade for contributing a wide range of shaders to the official library. Software design by Lautaro Nuñez Muller.

# Key Features

<ul>
  <li>Real-time execution of visuals using GLSL fragment shaders.</li>
  <li>Parameterized and generative visuals.</li>
  <li>Extremely lightweight for optimal performance.</li>
  <li>Possibility to add new shaders to expand the library.</li>
  <li>Communication with other programs through NDI and SPOUT.</li>
  <li>Parameter automation for creating smooth animations.</li>
  <li>Live audio-reactive parameters with FFT bands, onset detection, tempo tracking, calibration, and response shaping.</li>
  <li>OSC control for connections with external interfaces.</li>
  <li>MIDI mapping with learn mode and per-device profiles (all devices usable at once).</li>
  <li>Support for images, videos, and webcam as visual sources.</li>
  <li>Parameter randomization for surprising visual results.</li>
  <li>Built-in shader browser with live search, favorites, and preview.</li>
  <li>Integrated GLSL code editor with live compile/reload.</li>
  <li>Box groups (sub-compositions) and a cue/crossfade staging workflow.</li>
  <li>Integrated PAINT canvas with frame animation, layers, non-destructive selection and PNG/GIF export.</li>
  <li>Cross-platform: Windows, Linux and macOS (openFrameworks 0.12.1).</li>
</ul>

## Audio-Reactive Visuals

Enable audio in **SETTINGS**, choose an input device and channel mode, then use
the audio button on any eligible parameter. Sources include low/mid/high bands,
overall level, kick/snare envelopes, triggers, and logic. The compact
**Shaping** section controls amount, threshold, curve, polarity, attack, and
release. The inspector is content-sized and scrolls only when its content no
longer fits the window.

Shaders can consume normalized bands, onsets, rhythm data, and sixteen spectrum
bins through global uniforms. See [Audio-Reactive Visuals](mds/AUDIO_REACTIVITY.md)
for the complete workflow, uniform reference, diagnostics, and tests.

## PAINT Canvas

The integrated PAINT box supports drawing, frame-by-frame animation, onion
skin, layers and precise selection transforms. See the [PAINT guide](mds/PAINT.md)
for its workflow, shortcuts, export options and file compatibility notes.

## Release preparation

User-data isolation, checked storage, recovery and the release pipeline are documented
in [Publishing Guipper](mds/PUBLICACION.md). F10 opens version, update preferences
and diagnostic export. Development builds have updates disabled until signed
packages and platform validation are configured.

## Installation
This checkout targets openFrameworks **0.12.1**, C++17 and OpenGL 3.2.
Place it at `apps/myApps/Guipper` inside an openFrameworks installation:
`config.make` resolves `OF_ROOT` as `../../..`. Install the platform dependencies
required by that openFrameworks distribution and place `ofxNDI`, `ofxOsc` and
`ofxMidi` in its `addons/` directory. NDI also requires the runtime/libraries
expected by your installed `ofxNDI` addon.

From the project directory on Linux:

```bash
make Release -j2
./bin/launch-guipper.sh
```

The Makefile also contains a macOS post-build step to copy `bin/data` into the
application bundle. Windows project files are `guipper.sln` and
`guipper.vcxproj`; Spout is enabled only on Windows. These platform build paths
are present in the repository; this documentation update did not validate a
full application build on any platform.

Core tests run independently of the graphics application:

```bash
make -C tests run
```

### GPU selection on Linux

Launch Guipper through `bin/launch-guipper.sh`. It automatically uses NVIDIA
PRIME render offload when a working NVIDIA GPU is available and otherwise uses
the system default renderer. The desktop entry in `bin/Guipper.desktop` uses
this launcher too.

Set `GUIPPER_GPU=default` to use the system renderer explicitly, or
`GUIPPER_GPU=nvidia` to force NVIDIA PRIME offload:

```bash
GUIPPER_GPU=default ./bin/launch-guipper.sh
GUIPPER_GPU=nvidia ./bin/launch-guipper.sh
```

Set `GUIPPER_PROFILE=1` to show and log the CPU timing profiler while Guipper
is running.

### Kinect v2 on Linux

Guipper can capture Kinect v2 color, depth, and infrared directly through
`libfreenect2`; TouchDesigner, Spout, and an NDI bridge are not required.

```bash
./scripts/install_kinect2_linux.sh
```

The script installs a pinned `libfreenect2` revision in `/usr/local`, installs
the Kinect udev rules, and prints the command used to test all streams with
`Protonect`. The Kinect v2 requires its powered adapter and a direct USB 3
connection. Reconnect the device after installing the rules, verify Protonect,
then rebuild Guipper with `make -j2`.

When `pkg-config --exists freenect2` succeeds, Guipper enables native Kinect v2
capture automatically. Otherwise it still builds, and saved Kinect boxes show
an installation-required status. In the node view, press `Shift+C` to add a
Kinect box. Add three boxes and select `COLOR`, `DEPTH`, and `IR` in their
inspectors; all three share one device connection.

## User Guide
1. Use **IMPORT** to search and preview shaders, then load one into the graph.
2. Select a node to edit its parameters in the inspector. Connect an output to
   another node's texture input to build an effect chain.
3. Double-click a node to select the active render. Groups contain their own
   graph and active render; cue staging lets you prepare changes and apply a crossfade.
4. Open **EDITOR** through the shader's edit action. It supports multiple tabs,
   GLSL highlighting, selection, scroll and zoom. Saving writes the shader file
   and the graph's file watcher reloads it.
5. Use **SETTINGS** for audio, OSC and live outputs; MIDI supports learn mode
   and device profiles. **HELP** contains the keyboard reference and ES/EN help.

For detailed workflows, see [audio](mds/AUDIO_REACTIVITY.md),
[PAINT](mds/PAINT.md) and the [technical handbook](mds/SKILLS.md).

## Contributions
Report reproducible bugs or propose focused changes using the repository issue templates. For code changes, describe the affected workflow and the checks performed; preserve compatibility with existing XML compositions.

## License
This program is distributed under the [MIT License](LICENSE).

## Contact
You can reach me at julian.d.puppo@gmail.com.

## Roadmap

See the [current backlog](mds/FEATURE_BACKLOG.md), which separates implemented
features from proposed work.

-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#  Guipper.Software para Visuales en Tiempo Real

## Breve descripción del proyecto y su propósito.

Guipper es un software desarrollado para la realización de visuales en tiempo real utilizando GLSL fragment shaders. Permite ejecutar visuales parametrizados y generativos con un peso extremadamente bajo, además de brindar la posibilidad de agregar nuevos shaders.

Agradecimientos especiales a Kali Shade por contribuir con una amplia cantidad de shaders a la biblioteca oficial.
Diseño del software realizado por Lautaro Nuñez Muller.

## Características principales
<ul>
  <li>Ejecución de visuales en tiempo real utilizando GLSL fragment shaders.</li>
  <li>Visuales parametrizados y generativos.</li>
  <li>Peso extremadamente bajo para un rendimiento óptimo.</li>
  <li>Posibilidad de agregar nuevos shaders para ampliar la biblioteca.</li>
  <li>Comunicación con otros programas mediante NDI y SPOUT.</li>
  <li>Automatización de parámetros para crear animaciones fluidas.</li>
  <li>Parámetros audio-reactivos con FFT, detección de kick/snare, tempo y controles de respuesta.</li>
  <li>Control OSC para establecer conexiones con interfaces externas.</li>
  <li>Soporte para imágenes, videos y cámara web como fuentes visuales.</li>
  <li>Randomización de parámetros para obtener resultados visuales sorprendentes.</li>
  
</ul>

## Instalación
Usá openFrameworks **0.12.1** con sus dependencias de plataforma y los addons
`ofxNDI`, `ofxOsc` y `ofxMidi`. Este checkout espera estar en
`apps/myApps/Guipper`, con openFrameworks tres directorios por encima.
NDI requiere las bibliotecas que indique el addon instalado.

En Linux, desde la raíz del proyecto:

```bash
make Release -j2
./bin/launch-guipper.sh
```

Para las pruebas del núcleo: `make -C tests run`. Consultá la sección
[Installation](#installation) para las rutas de compilación de las otras plataformas.

## Guía de uso
Desde **IMPORT**, buscá y cargá un shader. Seleccioná el nodo para editar sus
parámetros; conectá su salida a las entradas de textura de otros nodos para
componer efectos. El doble clic elige el render activo. El editor GLSL permite
abrir varias pestañas, editar y guardar para activar la recarga del shader.

También hay grupos, cue/crossfade, mapeo MIDI con aprendizaje, audio reactivo,
PAINT, mapping y múltiples salidas. **HELP** incluye ayuda ES/EN y atajos.
Consultá las guías de [audio](mds/AUDIO_REACTIVITY.md), [PAINT](mds/PAINT.md)
y el [mapa técnico](mds/SKILLS.md).

## Contribuciones
Para reportar errores, incluí pasos para reproducirlos. Para cambios de código, describí el flujo afectado y las pruebas realizadas; conservá la compatibilidad con composiciones XML existentes.

## Licencia
Este programa se distribuye bajo la [licencia MIT](LICENSE).

## Contacto
Escribime al mail : julian.d.puppo@gmail.com
## Roadmap

El [backlog actualizado](mds/FEATURE_BACKLOG.md) distingue lo implementado
de las mejoras propuestas.
