# GUIPPER 4 - Análisis Completo de la Aplicación

## 1. DESCRIPCIÓN GENERAL

**Guipper 4** es una aplicación de **video mezcla / VJing en tiempo real** desarrollada en **C++ con openFrameworks**. Permite crear composiciones visuales en vivo combinando múltiples fuentes (shaders GLSL, imágenes, videos, cámara, Spout, NDI) en un editor de nodos visual, con control por OSC, MIDI y teclado.

Desarrollada por **JPUPPER** (jpupper), última modificación: 7/5/2021 (con actualizaciones posteriores al 2026).

---

## 2. ARQUITECTURA GENERAL

### 2.1 Plataforma
- **openFrameworks v0.11.2** (VS2017)
- **OpenGL 3.2** core profile
- **Windows** (con soporte condicional para Spout/DirectX)
- Build: Visual Studio 2017 (`.vcxproj`)

### 2.2 Addons Utilizados
| Addon | Propósito |
|-------|-----------|
| `ofxOsc` | Comunicación OSC (input/output) |
| `ofxMidi` | Control MIDI |
| `ofxNDI` | Transmisión NDI (video por red) |
| SpoutSDK | Compartir texturas GPU entre aplicaciones en Windows |

### 2.3 Macros de Compilación (defines.h)
```cpp
#define NDI           // Habilita soporte NDI
#define SPOUT         // Solo Windows: Spout texture sharing
#define RELATIVEDIRS  // Rutas relativas al data folder
```

---

## 3. ESTRUCTURA DE DIRECTORIOS

```
guipper4/
├── src/
│   ├── main.cpp                    # Entry point
│   ├── ofApp.h / ofApp.cpp         # Aplicación principal
│   ├── defines.h                   # Macros de compilación
│   ├── fix_line.cpp                # Utilidad
│   ├── JPbox/                      # Sistema de nodos (boxes)
│   │   ├── jp_box.h / .cpp         # Clase base JPbox
│   │   ├── jp_box_shader.h / .cpp  # Box de shader GLSL
│   │   ├── jp_box_image.h / .cpp   # Box de imagen
│   │   ├── jp_box_video.h / .cpp   # Box de video
│   │   ├── jp_box_cam.h / .cpp     # Box de cámara
│   │   ├── jp_box_spout.h / .cpp   # Box Spout receiver
│   │   ├── jp_box_ndi.h / .cpp     # Box NDI receiver
│   │   ├── jp_box_preset.h / .cpp  # Box preset (sub-composición)
│   │   ├── jp_box_sequencer.h/.cpp # Box secuenciador
│   │   ├── jp_box_framedifference.h/.cpp # Box diferencia de frames
│   │   └── JPboxgroup.h / .cpp     # Gestor de todos los nodos
│   ├── JPgui/                      # GUI widgets
│   │   ├── jp_slider.h / .cpp      # Slider
│   │   ├── jp_knob.h / .cpp        # Knob
│   │   ├── jp_toogle.h / .cpp      # Toggle (on/off)
│   │   ├── jp_tooglelist.h / .cpp  # Lista de toogles
│   │   ├── jp_bang.h / .cpp        # Botón momentáneo
│   │   ├── jp_complexslider.h/.cpp # Slider complejo
│   │   ├── jp_controller.h / .cpp  # Controller (LERP automation)
│   │   ├── jp_exposebutton.h/.cpp  # Botón "exponer" parámetro
│   │   └── jp_shader_editor.h/.cpp # Editor de código GLSL integrado
│   ├── JPutils/                    # Utilidades
│   │   ├── jp_constants.h / .cpp   # Constantes globales + tipografía
│   │   ├── jp_parametergroup.h/.cpp# Grupo de parámetros (float/bool)
│   │   ├── jp_fbohandler.h / .cpp  # Manejador de conexiones FBO
│   │   ├── jp_dragobject.h / .cpp  # Objeto arrastrable base
│   │   ├── jp_fileloader.h / .cpp  # Carga asíncrona de archivos
│   │   ├── jp_midi_keymap.h / .cpp # Mapeo MIDI
│   │   └── TransitionSR.h / .cpp   # Transición entre shaders
│   └── SpoutSDK/                   # SDK Spout (Windows)
├── bin/data/
│   ├── settings.xml                # Configuración persistente
│   ├── common.frag                 # GLSL header con utilidades
│   ├── shaders/
│   │   ├── default.vert            # Vertex shader por defecto
│   │   ├── private/                # Shaders internos del sistema
│   │   │   ├── sequencer.frag
│   │   │   ├── mix.frag
│   │   │   └── framedifference.frag
│   │   ├── generative/             # Shaders generativos
│   │   ├── imageprocessing/        # Shaders de procesamiento de imagen
│   │   ├── blending/               # Shaders de mezcla
│   │   └── contrib/                # Shaders contribuidos
│   ├── savefiles/                  # Composiciones guardadas (.xml)
│   │   ├── justvisuals/
│   │   ├── artlab/
│   │   ├── algorave/
│   │   ├── dubai/
│   │   └── foster/
│   ├── data/groups/                # Grupos guardados
│   ├── exportimgs/                 # Screenshots exportados
│   ├── font/
│   │   ├── Montserrat-Regular.ttf  # Fuente principal
│   │   ├── Montserrat-Medium.ttf   # Fuente para modales
│   │   └── consola.ttf             # Fuente monoespaciada (editor)
│   └── vid/                        # Videos de ejemplo
│       └── kinect.mov
└── mds/                            # Documentación del proyecto
    └── PLAN_EDITOR_SHADERS.md
```

---

## 4. SISTEMA DE PANTALLAS (Screen Tabs)

La app tiene 5 pantallas principales accesibles desde la barra superior:

| Tecla | Pantalla | Propósito |
|-------|----------|-----------|
| 1 | **NODOS** (NODES) | Editor de nodos visual - pantalla principal |
| 2 | **SETTINGS** (OPCIONES) | Configuración de la aplicación |
| 3 | **HELP** (TUTORIAL) | Instrucciones (bilingüe ES/EN) |
| 4 | **IMPORT** (SHADER_INDEX) | Navegador de shaders |
| 5 | **EDITOR** (EDITOR) | Editor de código GLSL integrado |

---

## 5. CORE: ofApp

### 5.1 Ciclo de Vida
- **`setup()`**: Inicializa fuentes, shader editor, OSC, MIDI, NDI, Spout, carga settings, carga composición por defecto
- **`update()`**: Actualiza nodos, MIDI, OSC, NDI/Spout output, preview del shader browser
- **`draw()`**: Renderiza la pantalla activa (NODOS, SHADER_INDEX, EDITOR, etc.)
- **`exit()`**: Guarda settings y cierra MIDI

### 5.2 Gestión de Eventos
- **`keyPressed()`**: Atajos de teclado, input de texto para modales/campos
- **`keycodePressed()`**: Ctrl+C/V (copiar/pegar), Ctrl+S (save-as o guardar shader)
- **`dragEvent()`**: Drag & drop de archivos al canvas
- **`mousePressed/Dragged/Released/Scrolled()`**: Input del usuario
- **`openRenderWindow()`**: Abre ventana secundaria de render

### 5.3 Configuración (settings.xml)
Persiste en `bin/data/settings.xml`:
- Render resolution (1920x1080 default)
- OSC ports y IP
- BPM
- Spout/NDI enabled
- Posición y tamaño de ventana de render
- Ruta de composición por defecto
- Layout del CUE panel
- Gallery duration

---

## 6. SISTEMA DE NODOS (JPbox / JPboxgroup)

### 6.1 JPbox - Clase Base
Todo nodo visual hereda de `JPbox` (que a su vez hereda de `JPdragobject`):

**Propiedades:**
- Posición (x, y), tamaño (width, height)
- **onoff** (Pause): toggle para pausar/reanudar
- **bypass**: toggle para pasar la entrada sin procesar
- **FBO** interno (resolución de render)
- **JPParameterGroup**: parámetros float/bool del nodo
- **JPFbohandlerGroup**: conexiones de entrada (FBOs de otros nodos)
- **Outlet** (triángulo amarillo): conexión de salida

**Tipos de Nodo (enum):**
```cpp
enum type {
    SHADERBOX,           // Shader GLSL
    IMAGEBOX,            // Imagen estática
    VIDEOBOX,            // Video
    CAMBOX,              // Cámara en vivo
    SPOUTBOX,            // Spout receiver (Windows)
    PRESETBOX,           // Sub-composición anidada
    FRAMEDIFFERENCEBOX,  // Diferencia entre frames
    NDIBOX,              // NDI receiver
    SEQUENCERBOX         // Secuenciador de slots
};
```

### 6.2 JPbox_shader - Nodo de Shader GLSL
- Carga y ejecuta un shader `.frag`
- **Parseo automático de uniforms**: escanea el código GLSL en busca de `uniform float`, `uniform bool`, `uniform sampler2D`, `uniform sampler2DRect`
- Crea sliders automáticos para cada `uniform float`
- Crea entradas (inlets) para cada `uniform sampler2D`
- **Auto-reload**: detecta cambios de `last_write_time` y recarga automáticamente
- **ShowCode**: overlays de texto en el FBO (modo debug)

**Uniformes globales automáticos:**
```glsl
uniform float time;
uniform vec2  resolution;
uniform float bpm;
uniform vec4  mouse;
uniform vec2  window_mouse;
uniform int   globalframeNum;
uniform int   boxframeNum;
uniform sampler2D feedback;  // Auto-feedback del propio FBO
```

### 6.3 JPbox_image - Nodo de Imagen
- Carga imágenes `.jpg`, `.png`, `.jpeg`
- Parámetros: scalex, scaley, offsetx, offsety, strech
- Auto-reload cada 2 segundos si falló la carga inicial

### 6.4 JPbox_video - Nodo de Video
- Reproduce `.mov`, `.mkv`, `.mp4`, `.flv`, `.vob`, `.avi`
- Parámetros: scalex, scaley, offsetx, offsety, strech, speed, position, play
- Loop normal por defecto, sin volumen

### 6.5 JPbox_cam - Nodo de Cámara
- Captura de cámara en vivo
- Parámetros: scalex, scaley, offsetx, offsety, camaraindex, strech
- Selección de dispositivo por slider (mapea al array de dispositivos disponibles)
- Refresh de dispositivos al iniciar

### 6.6 JPbox_spout - Nodo Spout Receiver (Windows)
- Recibe textura compartida via Spout desde otra aplicación

### 6.7 JPbox_ndi - Nodo NDI Receiver
- Recibe video via NDI desde la red

### 6.8 JPbox_preset - Nodo de Sub-Composición
- **CRUCIAL**: Permite anidar composiciones dentro de composiciones
- Carga un `.xml` de sesión como un nodo contenedor
- Sus hijos se renderizan secuencialmente
- **ExposedParams**: sistema para "exponer" parámetros de hijos al nivel superior
- **Viewport zoom/pan** por preset
- Guardado recursivo (cada preset guarda su propio XML)

### 6.9 JPbox_sequencer - Secuenciador de Slots
- Múltiples slots (hasta MAX_SLOTS)
- Cada slot es un `JPbox_shader` interno
- Botones +/- para agregar/quitar slots
- Transición entre slots (shader interno `sequencer.frag`)
- Parámetros: currentIndex, transition
- Auto-redimensionamiento según cantidad de slots

### 6.10 JPbox_framedifference - Diferencia de Frames
- Compara el frame actual con el anterior
- Shader interno `framedifference.frag`
- Parámetros: limit, force, origcolor
- Útil para detección de movimiento

---

## 7. JPboxgroup - Gestor de Nodos

### 7.1 Funcionalidades Principales
- **Render activo**: selecciona qué nodo se envía al output final
- **Inspector**: panel lateral con sliders para el nodo seleccionado (`openguinumber`)
- **Conexiones**: arrastrar outlet de un nodo a inlet de otro
- **Viewport**: zoom (scroll) y pan (click medio) del canvas
- **Selección múltiple**: clic+arrastrar para seleccionar varios nodos
- **Tabs contextuales**: MAIN + pestañas para presets anidados (breadcrumbs)
- **Copy/Paste**: clipboard XML de nodos seleccionados (Ctrl+C/V)
- **Group selection**: agrupa varios nodos en un preset (tecla 'u')

### 7.2 Sistema de Tabs
- **Tab 0** = MAIN (vista principal)
- **Tab 1+** = presets hijos directos en el contexto actual
- **Breadcrumbs**: `activeGroupPath` permite navegar profundidad arbitraria
- **Tab renaming**: doble click en nombre de tab para renombrar
- Zoom/pan independiente por tab

### 7.3 Sistema de CUE
**Sistema de preview y edición segura** que permite:
- **CUE_NORMAL_PREVIEW**: seleccionar un nodo como "cue" (previsualización)
- **CUE_DRAFT_CHAIN**: modo borrador - clona el grafo de nodos para editar sin afectar el output en vivo
- Monitorización de output final o nodo específico
- **Dirty flags**: seguimiento de qué cambió (parámetros, bypass, links, añadidos, eliminados)
- **CUE Apply**: aplica los cambios del draft al grafo real
- Panel flotante redimensionable con posición persistente

**Estados de CUE:**
```cpp
enum CueMode {
    CUE_NONE,
    CUE_NORMAL_PREVIEW,
    CUE_DRAFT_CHAIN
};

enum CueDirtyFlag {
    CUE_DIRTY_PARAMS = 1 << 0,
    CUE_DIRTY_BYPASS_PAUSE = 1 << 1,
    CUE_DIRTY_LINKS = 1 << 2,
    CUE_DIRTY_ADDED = 1 << 3,
    CUE_DIRTY_DELETED = 1 << 4,
    CUE_DIRTY_STAGED_ACTIVE = 1 << 5
};
```

### 7.4 Gallery / Sequence Mode
- Modo de presentación automática: cambia entre nodos cada `durationGallery` ms
- Parámetro: durationGallery (ms) controlado por slider en inspector
- Métodos: `setDurationGalleryMs()`, `getDurationGalleryMs()`

### 7.5 Transiciones
- `TransitionSR`: transición interpolada entre dos FBOs (shader interno)
- Se usa al cambiar el render activo

---

## 8. SHADER INDEX (Import - Pantalla 4)

### 8.1 Escaneo de Shaders
`scanShaders()` escanea carpetas específicas (no recursivo):
- `shaders/` (root)
- `shaders/blending/`
- `shaders/contrib/`
- `shaders/generative/`
- `shaders/imageprocessing/`
- `savefiles/` (XML presets, 1 nivel de subdirectorio)

### 8.2 Interfaz
- **Split panel**: izquierda = árbol de carpetas, derecha = preview
- **Barra de búsqueda**: filtra por nombre de shader
- **Expandir/colapsar** carpetas con click
- **Preview en vivo**: renderiza el shader seleccionado con uniforms globales + texturas de preview
- **Botones**:
  - **LOAD**: agrega el shader al canvas (grid distribution)
  - **RDM**: randomiza todos los uniforms float del shader
  - **EDIT**: abre el shader en el editor de código
- **Scroll** con ruleta del mouse
- **Hit-box debug** (tecla 'H')

---

## 9. EDITOR DE SHADERS (Pantalla 5)

Editor de código GLSL integrado (`JPShaderEditor`):

### 9.1 Características
- **Multi-tab**: múltiples shaders abiertos simultáneamente
- **Syntax highlighting**: GLSL keywords, types, preprocessor, comments, numbers, strings, built-in functions, uniforms
- **Navegación**: click, arrow keys, Home/End, PageUp/Down
- **Scroll vertical y horizontal**
- **Zoom**: Ctrl + mouse wheel (10-28px font size)
- **Selección**: Shift + arrows, drag
- **Ctrl+S**: guarda el archivo y dispara auto-reload del nodo
- **Números de línea** en el margen izquierdo
- **Status bar**: línea:columna, indicador de modificado, zoom level
- **Indicador de modificado** (circulo en el tab)

### 9.2 Layout
```
┌─────────────────────────────────────┐
│ [Tab1 ●] [Tab2] [Tab3]  [X] [SAVE] │  ← Top bar + Tab bar
├─────────────────────────────────────┤
│ 1 │ uniform float time;             │
│ 2 │ void main() {                   │  ← Code area
│ 3 │   vec2 uv = ...                 │
├─────────────────────────────────────┤
│ Ln:3, Col:12  |  Zoom:16px  |  ⚫  │  ← Status bar
└─────────────────────────────────────┘
```

---

## 10. CONTROL OSC

### 10.1 Recepción (OSC In)
- Puerto configurable (default 5000)
- **Comandos OSC:**
  - `/load/(nombre)` → carga composición de `savefiles/`
  - `/setactiverender/(num)` → activa render
  - `/openguinumber/(value)` → control de nodo activo
  - `/(shader)/(param)` → control por nombre de shader y parámetro
  - `/v0`, `/v1`, etc. → parámetros del nodo activo (modo 2)

### 10.2 Transmisión (OSC Out)
Dos modos simultáneos:
1. **Modo 1**: envía TODOS los parámetros de TODOS los nodos, dirección = `nombreShader/parametro`
2. **Modo 2**: envía solo el nodo activo, dirección = `v0`, `v1`, etc.

---

## 11. CONTROL MIDI

`JPMidiKeymap`: sistema completo de mapeo MIDI.

### 11.1 Acciones Mapeables
```cpp
enum Action {
    BYPASS,
    PAUSE,
    SELECT_OPEN_BOX,
    PARAMETER,
    NEXT_SHADER,
    PREV_SHADER,
    SET_CUE_SHADER,
    SET_ACTIVE_SHADER,
    SET_ACTIVE_RENDER,
    NEXT_SHADER_GALLERY,
    PREV_SHADER_GALLERY,
    TOGGLE_GALLERY,
    ADD_SHADER_BOX
};
```

### 11.2 Características
- Múltiples dispositivos MIDI
- Aprendizaje (learning mode)
- Mapeo a parámetros específicos por índice
- Mapeo a acciones globales
- Add shader bindings (mapear un control MIDI para cargar un shader específico)
- Persistencia en `midi_keymap.xml`
- Panel de configuración visual (tecla 'k')
- CC High State tracking (para mensajes Note/CC)

---

## 12. OUTPUTS

### 12.1 Render Activo
El nodo seleccionado como `activerender` se dibuja en:
- **Ventana principal** (mini preview en pantalla NODOS)
- **Ventana de render separada** (tecla 'w')
- **Spout sender** (si activo)
- **NDI sender** (si activo)

### 12.2 Spout (Windows)
- Comparte textura GPU con otras aplicaciones
- Resolución configurable
- Activable/desactivable desde SETTINGS

### 12.3 NDI
- Transmite video por red vía NDI
- Async mode + Readback mode
- Activable/desactivable desde SETTINGS

### 12.4 Exportación de Imagen
- Tecla 'm': exporta el render actual a `exportimgs/export-DIA-MES-AÑO-HORA-MIN-SEG-.png`

---

## 13. VENTANA DE RENDER SECUNDARIA

- Tecla 'w' para abrir
- OpenGL 3.2, shared context con la ventana principal
- Resolución configurable
- Fullscreen con tecla 'f'
- Posición y estado persistente en settings.xml

---

## 14. WIDGETS GUI (JPgui)

| Widget | Propósito |
|--------|-----------|
| **JPSlider** | Slider horizontal para valores float (con LERP) |
| **JPKnob** | Control rotatorio |
| **JPToogle** | Botón on/off |
| **JPToogleList** | Lista de toogles (para modos) |
| **JPBang** | Botón momentáneo (trigger) |
| **JPComplexSlider** | Slider con sub-sliders |
| **JPController** | Controller para LERP automation |
| **JPExposeButton** | Botón "ojo" para exponer parámetros |

### Sistema de Parámetros (JPParameterGroup)
- Parámetros de tipo **FLOAT** o **BOOL**
- Tipos de movimiento: STANDART, OSC, GODER, GOIZQ, RANDOM
- LERP automation: `floatValue` → `floatLerpValue`
- Nombre, min, max, speed por parámetro

### Sistema de Conexiones (JPFbohandlerGroup)
- Cada inlet de un nodo es un `JPFbohandler`
- Almacena puntero al FBO de origen + nombre
- Se dibujan como círculos en el borde izquierdo del nodo
- Color verde = conectado, rojo = desconectado

---

## 15. GUARDADO/CARGA DE SESIONES

### 15.1 Formato XML
Las composiciones se guardan como XML en `savefiles/`:
```xml
<box>
    <nombre>miShader</nombre>
    <x>300</x>
    <y>200</y>
    <directory>shaders/generative/efecto.frag</directory>
    <onoff>true</onoff>
    <bypass>false</bypass>
    <parameters>
        <param>
            <name>intensity</name>
            <min>0.0</min>
            <max>1.0</max>
            <value>0.5</value>
            <movtype>0</movtype>
            <speed>0.1</speed>
        </param>
    </parameters>
    <fboslinks>
        <slot0>otroShader</slot0>
    </fboslinks>
</box>
<activerender>0</activerender>
```

### 15.2 Save-As Modal (Ctrl+S)
- Campo de texto para nombre
- Botones: SAVE (nuevo archivo), UPDATE (sobrescribir actual), CANCEL
- Pre-fill con el nombre del archivo actual

### 15.3 Default Compo
- Ruta configurable en SETTINGS
- Se carga automáticamente al iniciar la aplicación

---

## 16. ATALOS DE TECLADO COMPLETOS

| Tecla | Acción |
|-------|--------|
| 1 | Nodos (editor de nodos) |
| 2 | Settings (configuración) |
| 3 | Help (instrucciones) |
| 4 | Shader Index (importar shaders) |
| 5 | Editor de shaders |
| Esc | Cerrar shader index / editor |
| s | Guardar sesión |
| Ctrl+S | Save-as / Guardar shader en editor |
| l | Cargar sesión |
| t | Toggle: arrastrar XML como preset vs sesión completa |
| d | Debug info |
| r | Recargar shader activo |
| w | Abrir ventana de render |
| f | Fullscreen en ventana de render |
| m | Exportar screenshot |
| e | Toggle modo secuencia (gallery) |
| q | Agregar sequencer box |
| u | Agrupar cajas seleccionadas |
| z | Toggle cue (selección rápida) |
| x | Trigger código en shader activo |
| h | Agregar Spout input (o debug hitboxes en shader index) |
| c | Agregar cámara |
| n | Agregar NDI receiver |
| i | Agregar Frame Difference |
| k | Abrir/cerrar panel MIDI Keymap |
| DEL | Eliminar shader seleccionado |
| Ctrl+C | Copiar cajas seleccionadas |
| Ctrl+V | Pegar cajas |

---

## 17. FLUJO DE RENDERIZACIÓN

```
Cada frame:
1. JPboxgroup::update()
   └─ Para cada nodo activo (en orden inverso):
      └─ box->update()
         └─ box->updateFBO()
            └─ Si bypass: pasar textura de entrada
            └─ Si paused (onoff=false): dibujar FBO anterior
            └─ Sino: ejecutar shader/dibujar imagen/video/cam
               └─ setUniforms globales (time, resolution, mouse, etc.)
               └─ setUniforms de parámetros
               └─ bind texturas de entrada (FBOs conectados)

2. JPboxgroup::draw_activerender()
   └─ boxes[activerender]->fbo.draw()

3. Output:
   └─ Ventana principal (mini preview)
   └─ Ventana de render (si abierta)
   └─ Spout sender (si activo)
   └─ NDI sender (si activo)
```

---

## 18. SHADERS INCLUIDOS (parcial)

### Private (internos del sistema)
- `sequencer.frag` - Mezcla de slots del secuenciador
- `mix.frag` - Shader base para slots
- `framedifference.frag` - Diferencia de frames

### Generative
- `aesteticpolys.frag`, `metro.frag`, `voronoiinvoronoi.frag`

### Image Processing (80+ shaders)
Categorías:
- **Blur/Feedback**: bloom, bloom2, bloom3, feedback_advance, feedbackmix, feedbacklimit, radialblur
- **Color**: blackandwhite, brightcontrast, chromakey, hue rotate, saturation, recolor
- **Displacement**: displace3d, feedbacklimitdisplace, planedisplace
- **Effects**: ASCII shader, VHS tape effect, gameboyfy, kaleidoscope, pixelate (CGA), vignette
- **Geometric**: flip, rotate, scale, mirror, perspective, extruder
- **Matrix**: matrix iterations, CGA madness
- **Raymarching**: raymarchingclase8, 82, 9, 92, rays, lighting3D
- **Blending**: multiply, mod1, mod2

---

## 19. CONSTANTES Y RECURSOS GLOBALES

### jp_constants
- Render resolution (width, height)
- Window dimensions
- BPM
- Gallery duration
- Mouse position (main + render window)
- Font pointers (p_font, h_font, p2_font)
- Color palettes (CmouseOver, Cfront, Cback, Cactive)

### jp_constants_img
- Imágenes precargadas: outlet, handler, speed, timeline, etc.

---

## 20. LIMITACIONES Y BUGS CONOCIDOS

1. **Shader reload loop**: si un shader tiene 0 parámetros, entra en loop infinito (forzado `do{...} while(parameters==0)`)
2. **Spout resolution**: limitado a la resolución de la ventana (no puede usar resolución de render arbitraria)
3. **Encoding warnings**: caracteres especiales en comentarios de código
4. **Sin soporte nativo Linux/Mac**: Spout es Windows-only (manejado con #ifdef)
5. **Sin undo/redo**: el editor de shaders no tiene historial de cambios
6. **ofxNDI no incluido**: requiere addon externo (ofxNDI)
7. **No hay migración de datos**: cambios en estructura XML pueden romper sesiones viejas
