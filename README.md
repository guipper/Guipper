
# Guipper Real-time Visual Software

Documentation reviewed against the local source on 2026-09-12. See the
[technical handbook](mds/SKILLS.md), [architecture boundaries](mds/ARQUITECTURA.md)
and [current backlog](mds/FEATURE_BACKLOG.md).
Release notes: [Changelog](CHANGELOG.md).


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

## Screen capture (PANTALLA)

On the node canvas, press **Shift+S** to add PANTALLA. Select it and click
**Pantalla completa** or **Ventana** in the inspector, then **Seleccionar pantalla**
or **Seleccionar ventana**. Linux and macOS use the system's sharing dialog; Windows
shows available monitors or titled application windows in the inspector. Connect
the output to a shader or mark it **TO OUTPUT**, just like a camera box.

**Cambiar pantalla** keeps the old feed until the new monitor delivers a frame.
Cancelling keeps the previous selection. While a system picker is pending,
**Cancelar seleccion** closes it so you can retry without losing the old feed.
Linux sessions use separate portal connections and attach the picker to Guipper.
Closed sessions and portal restarts release the pending selector immediately.
Non-interactive portal replies time out after 15 seconds; an unanswered picker
closes after two minutes and allows another attempt. You can cancel sooner with
**Cancelar seleccion**. Cancellation sends Close to the actual request handle
before disconnecting, including portals that return a different handle.
If the system portal itself cannot display its dialog, Guipper reports the
failure and permits retries; it does not restart the desktop's sharing service.
If a resolution change is followed by two seconds without frames, capture makes
one reconnect attempt to the same authorized stream. It does not reopen the
selector or switch sources. Ordinary static windows do not trigger retries.
**Detener**, disabling the box or its
parent group, deleting it, and closing Guipper release capture resources.
Undo, duplication and loading a saved composition never start sharing: select
the source again. Transform parameters and the screen/window mode are saved. Screen sources restored
without a selection are transparent placeholders, so loading does not wait for
an unsolicited permission dialog.

Capture includes the cursor, preserves aspect ratio by default, and processes
at most 30 frames per second. Each box owns an independent session. Capturing
the monitor displaying Guipper can create a recursive mirror. This first version
captures one monitor or one fixed window per box; it does not capture audio or
YouTube URLs. Changing mode stops capture and clears the output until you select
the new source. Window capture does not follow the currently focused application.
Minimized windows may stop delivering frames, depending on the operating system. Output is SDR RGBA; HDR fidelity is not guaranteed.

Platform requirements:

- **Linux:** a desktop portal implementing ScreenCast, PipeWire and GStreamer's
  `pipewiresrc` plugin (`gstreamer1.0-pipewire` on Ubuntu), plus the existing GTK/GIO
  and GStreamer development dependencies. This works from Guipper's XWayland window.
- **Windows:** Windows 10 version 1903 or newer, D3D11, and a Windows SDK with C++/WinRT
  headers. Build with Visual Studio 2022 and Windows SDK 10.0.19041 or newer.
- **macOS:** macOS 14+, built with a macOS 14+ SDK, ScreenCaptureKit, CoreMedia and
  CoreVideo. Grant the screen-sharing permission requested by the system.

Tests: `make -C tests run` includes the platform-independent capture state tests.
On Linux, `make -C tests screen_portal_tests && ./tests/screen_portal_tests` runs
the real backend against a private mock D-Bus portal: repeated selection,
non-predicted handles, cancellation, session closure, service restart, timeout,
and retry. It does not open dialogs or change the desktop portal.
The `GUIPPER_PERSISTENCE_TEST=screen_capture` graphics test must run from a copy
of `bin/data`; it tests rendering, serialization, duplicate and delete/undo/redo
without opening the system picker, including mode changes, stale frames and mode
persistence. `GUIPPER_UISHOT=screen_capture` (or `window_capture`) checks the
creation shortcut and captures the inspector after the first rendered frames;
run it from the same disposable copy. Adding `GUIPPER_CAPTURE_LIVE=1` opens the
real picker and checks pixel totals in the source and rendered FBO (no shared
contents are saved); select a visible, nonblack window. Native Windows/macOS builds and permission,
monitor-removal and multi-monitor checks still require those operating systems.

## YouTube video box

On the node canvas, press **Shift+Y**, paste a single YouTube video URL in the
inspector and click **Reproducir streaming**. Connection runs in the background
and can be cancelled. Once ready, connect the box to a shader or use **TO OUTPUT**.
The existing VIDEO controls provide play/pause, loop, position, speed and fit.
The default fit preserves proportions; audio starts muted.

Playback uses streaming by default (up to 1080p). **Descargar en cache** optionally
prepares a local MP4 (H.264/AAC, 2 GB maximum) for unreliable connections. Streaming
continues during download; once complete, playback switches to the local copy
while preserving its position. **Usar cache** reuses an existing copy;
**Reproducir streaming** switches back to the network. Separate HLS audio/video
tracks are joined using a temporary playlist; a video-only fallback is labelled.
Audio support for streaming depends on the platform player and codecs.
It supports ordinary public videos, not live streams,
playlists, account login or restricted videos. YouTube availability can change;
a failed connection or download reports an error and can be retried. No browser cookies
or user yt-dlp configuration are read.

Dependencies: current [yt-dlp](https://github.com/yt-dlp/yt-dlp#installation),
ffmpeg, and a supported JavaScript runtime (Node.js or Deno; see
[yt-dlp EJS requirements](https://github.com/yt-dlp/yt-dlp/wiki/EJS)).
Install/update an official, checksum-verified copy of yt-dlp for Guipper only:

```sh
python3 scripts/install-youtube-helper.py
```

The helper lives in Guipper's cache `tools/` directory. Guipper uses it before
searching `PATH`; `GUIPPER_YTDLP` can select an explicit executable path. The
installer does not replace the system yt-dlp or install ffmpeg/JavaScript runtimes.
It requires network access; the app never downloads helper executables itself.

Prepared videos live under Guipper's `cache/youtube/` (normally
`~/.cache/guipper/youtube` on Linux). Compositions save the canonical URL and
media/transform settings and the selected playback mode, never signed stream URLs
or running jobs. Load, duplicate and undo reconnect streaming boxes when enabled;
local-mode boxes reuse an existing cache entry without starting a download. A
missing entry requires **Descargar en cache** or switching to streaming.
Deleting/disabling a box, disabling its parent group or closing the app stops
playback, cancels background processes and removes temporary files. Prepared entries remain reusable; remove them with Guipper closed to
reclaim disk space. The cache is local to each computer, not embedded in a composition.

Tests: `make -C tests youtube_tests && ./tests/youtube_tests` checks URL handling,
cache/error behavior, stream resolution, HLS/audio, cancellation, late results and process cleanup.
`python3 tests/youtube_graphics_test.py` runs isolated persistence and graphics
checks against the compiled Linux app, including duplicate/delete/undo/redo,
animated cached playback and a shader connection. Add `--stream --url URL` to
verify real network streaming followed by optional cache download. Windows and macOS build/runtime
validation remains pending on their respective machines.

## Text box

Press **Shift+T** on the node canvas to create **TEXTO**. Its transparent output
connects to shaders, TO OUTPUT and the FINAL stack like other sources.

The inspector provides a multiline UTF-8 editor with selection, clipboard,
local undo/redo and automatic visual wrapping. Enter adds a line; Ctrl/Cmd+Enter
finishes editing; Escape restores the text from the start of that edit. Node
shortcuts are suppressed while a field is focused. Text is limited to 8 KiB.

**En vivo** publishes typing immediately. **Preparar texto** keeps a separate
draft; **Aplicar cambios** publishes it, and **Descartar borrador** restores the
current output. This mode applies to the text content; typography controls
continue to update the output immediately. Both versions and the chosen mode
survive save/load and duplication. Completed text edits, Apply, font and
alignment changes participate in graph undo/redo outside the editor.

Choose/search a font or **Importar fuente** (.ttf/.otf, up to 32 MB). Imports
are copied to the user profile's `data/text-fonts/`; the original can then move.
These fonts are not embedded in compositions: copy that directory to another
computer or import the font there. A missing imported font uses a fallback
and displays a warning. Existing compositions and backups are not migrated.

Controls cover font size/weight, italic, block width, position, horizontal alignment, vertical
anchor (relative to the position), line and letter spacing, padding, RGB/opacity,
outline, shadow and optional background. A clipping warning appears when text
exceeds the canvas. Output is rerendered only when its content, style or render
resolution changes, on the render thread.

Linux uses Pango/Cairo and Fontconfig for shaping, wrapping and system font
fallback (development packages: `libpango1.0-dev`, `libcairo2-dev`,
`libfontconfig1-dev`; already supplied by typical openFrameworks GTK setups).
The Windows/macOS fallback uses openFrameworks FreeType with imported fonts and
Latin/Greek/Cyrillic ranges; complex script shaping, synthetic weight/italic, system fallback and soft
shadows currently require the Linux renderer. Windows/macOS build and runtime
validation remain pending. Emoji color fonts are not a guaranteed output format.

Validation: `make -C tests text_render_tests && ./tests/text_render_tests` tests
CPU layout, transparency, styles and resize on Linux.
`python3 tests/text_graphics_test.py` tests the compiled app using a temporary
profile and data copy, including persistence, duplication, deletion/undo/redo,
draft isolation and a shader connection. `tests/run_text_input.py` covers the
shared keyboard/clipboard routing. Animation and phrase sequences are future work.

## Installation
This checkout targets openFrameworks **0.12.1**, C++17 and OpenGL 3.2.
Place it at `apps/myApps/Guipper` inside an openFrameworks installation:
`config.make` resolves `OF_ROOT` as `../../..`. Install the platform dependencies
required by that openFrameworks distribution and place `ofxNDI`, `ofxOsc` and
`ofxMidi` in its `addons/` directory. NDI also requires the runtime/libraries
expected by your installed `ofxNDI` addon.

On Linux, apply the camera compatibility patch once to the openFrameworks SDK.
It excludes hardware-memory formats that the CPU capture pipeline cannot import
(including `DMA_DRM` advertised by newer GStreamer versions):

```bash
patch -d ../../.. -p1 --forward < patches/openframeworks/0001-camera-system-memory.patch
```

If the patch reports that it is already applied, proceed with the build.

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

## ANALOG COLOR (Shift+R)

Connect an image, camera, screen or video to `imagen`. The embedded GPU effect
reduces it to 2–16 colors from Analogica, Sepia or Grises palettes. `colors` rounds
to a whole palette size; a particular image may use fewer colors. `dither` controls
ordered 4×4 dithering, `grain` adds animated noise before quantization, and `bleed`
softens horizontal color transitions. Contrast and saturation are applied before
palette selection. `mix=0` preserves the original; `mix=1` limits RGB to the palette.
Intermediate mix values intentionally contain additional colors. Source alpha is
preserved at full resolution. `pixel size` reduces spatial resolution into square
blocks measured in output pixels (1–64, rounded to an integer); 1 keeps the
original resolution. Color and alpha are sampled at each block center, and grain
and dithering operate per block. The output dimensions stay unchanged; `mix=0`
bypasses pixelation too. This is a stylized analog look, not a calibrated hardware emulation.

Settings and the inlet persist through save/load, duplication and undo. No input
produces a transparent output. The shader is embedded and requires no new runtime
dependencies. `python3 tests/analogcolor_graphics_test.py` verifies GPU palette
counts, alpha, bypass orientation, dithering, persistence and the inspector.

## Unavailable box types

Compositions containing unavailable box types still open. Guipper skips those
boxes, leaves their incoming connections disconnected, and reports the omitted
names. Surviving connections, active output and exposed group parameters retain
their original targets. If the active box is omitted, the first surviving box
is selected; an entirely unavailable composition opens empty. Loading does not
rewrite the original XML. Saving the loaded composition omits the unavailable
boxes. Known missing assets and malformed compositions still report errors.
Run `python3 tests/missing_boxes_test.py` for isolated regression coverage.
