# Plan: Módulo EDITOR de Shaders para Guipper4

## Objetivo

Crear un editor de shaders integrado en guipper4 que permita editar código GLSL con:
- Syntax highlighting para GLSL
- Sistema de tabs (múltiples shaders abiertos simultáneamente)
- Scroll vertical y zoom in/out
- Ctrl+S para guardar y recargar automáticamente los nodos
- Botón EDIT en el shader index (junto a LOAD/RDM) y en el inspector

## Arquitectura

### Archivos nuevos

1. `src/JPgui/jp_shader_editor.h` — Clase `JPShaderEditor` (editor de código)
2. `src/JPgui/jp_shader_editor.cpp` — Implementación
3. `bin/data/font/Consolas.ttf` o similar (fuente monoespaciada)

### Archivos modificados

1. `src/ofApp.h` — Nuevos miembros para el editor (flag, instancia, tabs)
2. `src/ofApp.cpp` — Integración en draw, keyPressed, mousePressed, keycodePressed
3. `src/JPbox/JPboxgroup.h` — Botón EDIT en inspector
4. `src/JPbox/JPboxgroup.cpp` — Dibujar y manejar botón EDIT, lógica `draw_paramswindow()`
5. `src/JPbox/jp_box_shader.h` — Método `getFilePath()` expuesto
6. `*.vcxproj` — Agregar nuevos archivos al proyecto

## Diseño de JPShaderEditor

### Estado interno

```
vector<EditorTab> tabs;        // Tabs abiertas
int activeTab = -1;            // Tab activa (-1 = ninguna)
bool visible = false;          // Editor visible

struct EditorTab {
    string filePath;           // Ruta del archivo .frag
    string shaderName;         // Nombre para mostrar en el tab
    vector<string> lines;      // Líneas del código
    int cursorLine, cursorCol; // Posición del cursor
    int scrollLine;            // Scroll vertical (primera línea visible)
    float fontSize;            // Tamaño de fuente (zoom)
    bool modified;             // Modificado sin guardar
    JPbox_shader* targetBox;   // Puntero al nodo shader (puede ser nullptr)
    int targetBoxIndex;        // Índice en boxes[] (-1 si no)
};
```

### Layout visual

- Panel semi-transparente que ocupa 80% de la pantalla (como el shader index)
- Barra superior: nombre del shader + botón CERRAR (X) + botón GUARDAR
- Barra de tabs debajo: tabs con flechita de cierre individual
- Área de código con números de línea, syntax highlighting
- Barra inferior: info (línea:columna, zoom level, modificado)

### Syntax Highlighting (GLSL)

Colores:
- Keywords (void, float, vec2, if, return, etc.): #FF6188 (rosa/rojo)
- Types (float, int, vec2, vec3, vec4, mat2, sampler2D): #FFD866 (amarillo)
- Preprocessor (#ifdef, #define, #include): #A9DC76 (verde)
- Comments (//): #75715E (gris)
- Numbers: #AE81FF (púrpura)
- Strings: #E6DB74 (amarillo claro)
- Built-in functions (sin, cos, mix, fract, etc.): #66D9EF (cyan)
- Uniforms: #FD971F (naranja)
- Default: #F8F8F2 (blanco)

### Sistema de Tabs

- Flechita ✕ al lado del nombre de cada tab para cerrar
- Click en tab para activar
- Los tabs muestran "●" si el archivo está modificado
- Máximo ~10 tabs abiertos (los más viejos se cierran)

### Navegación y Edición

- Scroll: mouse wheel (afecta `scrollLine`)
- Zoom: Ctrl + mouse wheel (cambia `fontSize`, rango 10-28px)
- Cursor: click para posicionar, arrow keys para mover
- Home/End, Ctrl+Home/Ctrl+End
- PageUp/PageDown
- Edición: typing inserta caracteres, Backspace borra, Delete borra hacia adelante
- Enter: nueva línea
- Tab: inserta 4 espacios (configurable)
- Selección: Shift + arrow keys

### Ctrl+S

Cuando el editor está activo y visible:
1. Guardar el archivo modificado al disco con `ofBuffer::writeTo()`
2. Esperar un frame (el auto-reload en JPboxgroup::update detecta el cambio de `datemodified`)
3. El sistema de auto-reload YA EXISTENTE en JPboxgroup.cpp (líneas 994-1030) se encarga de:
   - Detectar el cambio de `last_write_time`
   - Llamar a `boxes[i]->reload()`
   - Llamar a `setControllers()` si el shader está abierto en el inspector
4. Marcar el tab como no modificado

**IMPORTANTE**: El Ctrl+S del editor debe tomar prioridad sobre el save-as modal existente.

## Integración

### 1. Shader Index — Botón EDIT

En `draw_shaderindex()`, junto a LOAD y RDM, agregar botón EDIT:

```
[ LOAD ] [ RDM ] [ EDIT ]
```

Click en EDIT → abre el shader seleccionado en el editor

### 2. Inspector — Botón EDIT

En `draw_paramswindow()`, si el inspectorBox es SHADERBOX, al lado del botón RDM agregar botón EDIT.

```
[NOMBRE DEL SHADER] [RDM] [EDIT]
```

Click en EDIT → abre el shader del nodo actual en el editor

### 3. Mouse Handling

Cuando el editor está visible:
- `mousePressed()`: detecta clicks en tabs (cambiar/cerrar), área de código (posicionar cursor), botones (SAVE, CLOSE)
- `mouseScrolled()`: scroll vertical o zoom (con Ctrl)
- `mouseDragged()`: selección de texto

### 4. Keyboard Handling

Cuando el editor está visible (se revisa ANTES que otros handlers):
- `keyPressed(int key)`: caracteres normales, Enter, Backspace, Delete, Tab, arrows, Escape
- `keycodePressed(ofKeyEventArgs &e)`: Ctrl+S (guardar y recargar), Ctrl+Z/Y (undo/redo — para versión futura)

## Orden de Ejecución

### Paso 1: Crear JPShaderEditor (.h y .cpp)
- Clase con toda la lógica del editor
- Carga de fuente monoespaciada (usar Consolas del sistema Windows: `C:\Windows\Fonts\consola.ttf`)
- Renderizado con syntax highlighting
- Manejo de tabs, scroll, zoom, cursor

### Paso 2: Integrar en ofApp.h y ofApp.cpp
- Agregar `JPShaderEditor shaderEditor` como miembro
- Agregar flag de visibilidad, fuente monoespaciada
- Inicializar en setup()
- Llamar draw, manejar input

### Paso 3: Modificar shader index (ofApp.cpp)
- Agregar botón EDIT en `draw_shaderindex()`
- Agregar handler en `mousePressed()`

### Paso 4: Modificar inspector (JPboxgroup.h/.cpp)
- Agregar `JPBang editbutton` como miembro
- Dibujar y manejar botón EDIT en `draw_paramswindow()`
- Handler en `update_mousePressed()`

### Paso 5: Modificar keycodePressed
- Ctrl+S redirige al editor cuando está activo

### Paso 6: Agregar archivos al vcxproj
- ClCompile y ClInclude para los nuevos archivos

## Riesgos y Consideraciones

1. **ofDrawBitmapString vs ofTrueTypeFont**: `ofDrawBitmapString` es monoespaciado nativo pero limitado en tamaño. Usar `ofTrueTypeFont` con Consolas para tener control de zoom.

2. **Ctrl+S conflicto**: El Ctrl+S actual abre el save-as modal. Cuando el editor está activo, Ctrl+S debe guardar el shader. Se resuelve con un early check en `keycodePressed()`.

3. **Auto-reload ya funciona**: El sistema existente en JPboxgroup::update detecta cambios de archivo por timestamp. Solo necesitamos escribir el archivo al disco.

4. **Performance**: Renderizar cientos de líneas con syntax highlighting podría ser pesado. Optimizar dibujando solo líneas visibles (las que están en pantalla según scroll y zoom).

5. **Sin addons externos**: No usar ImGui ni otros addons — mantener todo con OF nativo para no complicar el build.

6. **Encoding**: Los shaders pueden tener caracteres especiales (tildes en comentarios). Usar UTF-8.

## Verificación

- [ ] Editor abre con doble click en EDIT (shader index)
- [ ] Editor abre con click en EDIT (inspector)
- [ ] Syntax highlighting funciona (colores distintos para keywords, comments, etc.)
- [ ] Tabs: abrir 3 shaders distintos, cambiar entre tabs, cerrar tabs
- [ ] Scroll vertical con mouse wheel
- [ ] Zoom con Ctrl + mouse wheel
- [ ] Cursor: click para posicionar, arrows para mover, typing para editar
- [ ] Ctrl+S guarda y dispara reload (verificar que el nodo se actualiza visualmente)
- [ ] Escape/Close cierra el editor
- [ ] Números de línea en el margen izquierdo
- [ ] Indicador de modificado (● en el tab)
