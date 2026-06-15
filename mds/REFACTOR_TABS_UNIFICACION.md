# REFACTOR: Unificacion MAIN / GROUP VIEW (TABS)

## Objetivo General

Eliminar la duplicacion de funcionalidad entre la vista principal (MAIN) y las
vistas de grupo (TABS / boxgroup) en JPboxgroup. Actualmente cada operacion de
la interfaz existe en dos versiones paralelas: una que opera sobre `boxes` (el
vector principal) y otra que opera sobre `preset->boxes` (sub-cajas del preset
activo). El objetivo es tener UNA SOLA implementacion que funcione en ambos
contextos, determinando el vector activo en tiempo de ejecucion.

Esto asegura que cualquier modificacion futura (nuevos tipos de nodo, cambios
en UI, fixes de comportamiento) funcione automaticamente tanto en MAIN como en
cualquier TAB.

---

## Arquitectura Actual

JPboxgroup contiene:

- `boxes` : vector<JPbox*> — TODAS las cajas del proyecto (main)
- `activeGroupPath` : vector<int> — ruta al preset activo (vacio = main)
- `isGroupViewActive()` : `!activeGroupPath.empty()`
- `getActivePreset()` : devuelve el JPbox_preset* en `activeGroupPath`
- `preset->boxes` : vector<JPbox*> — sub-cajas dentro del preset

### Duplicaciones identificadas

| # | Area | MAIN | GROUP | Lineas |
|---|------|------|-------|--------|
| 1 | Selection rect flag | `draw_SelectionRect` | `groupDrawSelectionRect` | header |
| 2 | Selected indices | `selectedBoxIndices` | `groupSelectedIndices` | header |
| 3 | Clear selection | `clearSelection()` | `clearGroupSelection()` | 3895-3905 |
| 4 | Intersection test | `boxIntersectsSelection()` | `boxIntersectsGroupSelection()` | 3907-3959 |
| 5 | Update selection | `updateBoxSelection()` | `updateGroupSelection()` | 3927-3971 |
| 6 | Draw selection rect | `draw()` lines 223-238 | `drawGroupView()` lines 5080-5095 | ~30 c/u |
| 7 | Draw selected outline | `draw()` lines 304-314 | `drawGroupView()` lines 5120-5131 | ~10 c/u |
| 8 | Multi-drag | `mouseDragged` 1186-1207 | `mouseDragged` 1125-1146 | ~20 c/u |
| 9 | Conexiones outlet | `mouseDragged` 1209-1236 | `mouseDragged` 1150-1168 | ~25 c/u |
| 10 | MouseReleased final | `mouseReleased` 1586-1591 | `mouseReleased` 1571-1576 | ~5 c/u |
| 11 | Inspector index | `openguinumber` | `groupInspectorIndex` | header |
| 12 | Active render | `*activerender` | `preset->activeRender` | header |
| 13 | Zoom/Pan | `viewportZoom/viewportPan` | `tabZooms[activeTab]/tabPans[activeTab]` | header |

---

## Bugs Identificados que la refactorizacion corrige

### Bug A: No se pueden agregar nodos especiales en group view

En `addBox()` linea 3773:

```cpp
if (isGroupViewActive() && directory.find(".xml") != std::string::npos)
```

Solo `.xml` (presets) se agregan al preset activo. Spout, NDI, cam, y
framedifference se agregan al vector `boxes` principal pero son invisibles
porque el draw esta en modo group view.

Fix: sacar la condicion `.xml` para que TODO se agregue al preset activo
cuando se esta en group view.

### Bug B: Multi-seleccion con drag no funciona en group view

En `update_mousePressed()` (group view, linea 1340):
```cpp
if (hitBox && clickedIndex >= 0)
{
    clearGroupSelection();  // SIEMPRE limpia, incluso si la box ya estaba seleccionada
    groupInspectorIndex = clickedIndex;
```

En MAIN view (lineas 1505-1507):
```cpp
if (!isBoxSelected(i))
{
    clearSelection();  // Solo limpia si NO estaba seleccionada
}
```

El group view siempre descarta la seleccion previa al clickear, impidiendo
arrastrar un grupo ya seleccionado.

---

## Plan de Implementacion por Fases

### FASE 0: Preparacion (archivo de seguimiento) [ ]

- [ ] Crear este documento
- [ ] Marcar progreso a medida que se implementa

### FASE 1: Unificar sistema de seleccion [ ]

Objetivo: Reemplazar los 4 pares de funciones de seleccion por una sola
implementacion que opere sobre el vector activo.

Archivos afectados: JPboxgroup.h, JPboxgroup.cpp (lineas 3895-3976)

Cambios:

1. En JPboxgroup.h:
   - Mantener `selectedBoxIndices` y `draw_SelectionRect` (las originales)
   - Eliminar `groupSelectedIndices` y `groupDrawSelectionRect`
   - Eliminar declaraciones de `clearGroupSelection()`, `boxIntersectsGroupSelection()`, `updateGroupSelection()`

2. En JPboxgroup.cpp, agregar helper:
```cpp
vector<int>& JPboxgroup::getActiveSelectionIndices() {
    return isGroupViewActive() ? groupSelectedIndices : selectedBoxIndices;
}
```
   ESPERA: no, la idea es justamente ELIMINAR groupSelectedIndices.
   Mejor: usar SIEMPRE `selectedBoxIndices` y `draw_SelectionRect`.
   El unico cambio necesario es que `updateBoxSelection()` y demas
   iteren sobre el vector activo de boxes.

3. Refactor:
   - `clearSelection()` ya funciona (limpia `selectedBoxIndices` y `draw_SelectionRect`)
   - `clearGroupSelection()` -> eliminar, reemplazar llamadas por `clearSelection()`
   - `boxIntersectsSelection()` y `boxIntersectsGroupSelection()` -> fusionar en una sola `boxIntersectsRect()`
   - `updateBoxSelection()` y `updateGroupSelection()` -> fusionar, recibe vector<JPbox*>& como parametro

### FASE 2: Arreglar addBox para nodos especiales [ ]

Objetivo: Permitir agregar spout, ndi, cam, framedifference en group view.

Archivo: JPboxgroup.cpp, linea 3773

Cambio: Reemplazar:
```cpp
if (isGroupViewActive() && directory.find(".xml") != std::string::npos)
```
por:
```cpp
if (isGroupViewActive())
```

### FASE 3: Unificar update_mousePressed [ ]

Objetivo: Unificar la logica de click en boxes para main y group view,
incluyendo el fix de multi-seleccion con drag.

Archivo: JPboxgroup.cpp (lineas 1302-1538), JPboxgroup.h

Cambios:

1. Extraer la logica de "encontrar box clickeada" a una funcion auxiliar
   que reciba vector<JPbox*>& como parametro.

2. Unificar el bloque de seleccion: si la box clickeada ya estaba
   seleccionada, NO limpiar la seleccion. Esto aplica tanto a main como
   a group view.

3. Mantener separada la parte de inspector (`openguinumber` vs
   `groupInspectorIndex`) porque semanticamente son distintas
   (una apunta al vector global, la otra al vector del preset).

### FASE 4: Unificar update_mouseDragged [ ]

Objetivo: Extraer la logica de drag de multi-seleccion y conexiones
a funciones que reciban el vector activo como parametro.

Archivo: JPboxgroup.cpp (lineas 1095-1301)

Cambios:

1. Extraer `dragSelectedBoxes(vector<JPbox*>& boxes, vector<int>& selectedIndices,
   float deltaX, float deltaY)` — funcion que mueve todas las boxes
   seleccionadas.

2. Extraer `handleOutletConnections(vector<JPbox*>& boxes, ...)` —
   funcion que maneja las conexiones outlet-input.

3. En `update_mouseDragged()`, determinar el vector activo al inicio
   y usar las funciones extraidas.

### FASE 5: Unificar update_mouseReleased [ ]

Objetivo: Unificar el cierre del selection rect.

Archivo: JPboxgroup.cpp (lineas 1539-1592)

Cambio similar a Fase 1: usar `draw_SelectionRect` como unico flag
y `updateBoxSelection()` sobre el vector activo.

### FASE 6: Unificar draw() y drawGroupView() [ ]

Objetivo: Tener UNA sola funcion draw() que renderice tanto main como
group view, determinando el vector activo al inicio.

Archivo: JPboxgroup.cpp (lineas 195-344 y 5033-5138)

Cambios:

1. En `draw()`, reemplazar el `if (isGroupViewActive()) { drawGroupView(); return; }`
   por logica que determine el vector activo.

2. Extraer a funciones auxiliares:
   - `drawSelectionRect()` — dibuja el rectangulo de seleccion
   - `drawBoxConnections(vector<JPbox*>& boxes)` — dibuja las conexiones
   - `drawBoxSelectionOutline(JPbox* box)` — dibuja el outline cyan
   - `drawCueDraftOverlay(JPbox* box, int i)` — dibuja overlays de cue draft

3. Iterar sobre el vector activo con el mismo codigo.

4. Eliminar `drawGroupView()`.

### FASE 7: Unificar zoom/pan y limpiar header [ ]

Objetivo: Eliminar variables duplicadas del header y asegurar zoom/pan
funcione correctamente por tab.

Archivo: JPboxgroup.h, JPboxgroup.cpp

Cambios:

1. En JPboxgroup.h:
   - Eliminar `groupDrawSelectionRect` (usar `draw_SelectionRect`)
   - Eliminar `groupSelectedIndices` (usar `selectedBoxIndices`)
   - Eliminar declaraciones de funciones duplicadas
   - Mantener `groupInspectorIndex` (es semanticamente distinto)

2. En `handleTabClick()`, guardar/restaurar zoom/pan por tab. Esto ya
   esta implementado pero hay que verificar que `screenToCanvas()` use
   el zoom/pan correcto.

### FASE 8: Compilar y probar [ ]

- [ ] Compilar con MSVC / Visual Studio
- [ ] Verificar seleccion multiple + drag en MAIN
- [ ] Verificar seleccion multiple + drag en GROUP VIEW
- [ ] Verificar agregar spout/ndi/cam/framedifference en MAIN
- [ ] Verificar agregar spout/ndi/cam/framedifference en GROUP VIEW
- [ ] Verificar conexiones outlet-input en MAIN
- [ ] Verificar conexiones outlet-input en GROUP VIEW
- [ ] Verificar inspector panel en MAIN
- [ ] Verificar inspector panel en GROUP VIEW
- [ ] Verificar doble click para active render
- [ ] Verificar zoom/pan por tab

---

## Progreso

| Fase | Descripcion | Estado | Fecha |
|------|-------------|--------|-------|
| 0 | Documento de seguimiento | COMPLETED | |
| 1 | Unificar sistema de seleccion | COMPLETED | |
| 2 | Arreglar addBox nodos especiales | COMPLETED | |
| 3 | Unificar update_mousePressed | COMPLETED | |
| 4 | Unificar update_mouseDragged | COMPLETED | |
| 5 | Unificar update_mouseReleased | COMPLETED | |
| 6 | Unificar draw() y drawGroupView() | COMPLETED | |
| 7 | Limpiar header y zoom/pan | COMPLETED | |
| 9 | CUE en GROUP VIEW (TABS) | IN PROGRESS | |

---

## Notas de implementacion

### FASE 9: Hacer CUE funcionar en GROUP VIEW (TABS)

**Problema detectado:**
Todo el sistema CUE (toggleCueByIndex, setCueByIndex, beginCueDraftForBoxIndex, buildCueDraftGraph, cloneBoxForCueDraft, applyCueDraftToSource, etc.) opera SIEMPRE sobre el vector `boxes` (main). Cuando el usuario esta en group view y presiona 'z', se llama `toggleCueBoxByIndex(groupInspectorIndex)` que intenta operar sobre `boxes[groupInspectorIndex]` en vez de `preset->boxes[groupInspectorIndex]`. O referencias a indices incorrectos, o modifica boxes del main.

**Solucion: Parametrizar el sistema CUE segun el contexto**

Agregar a CueState un flag `targetPreset` que indique si el CUE opera sobre un preset. Crear helpers que devuelvan la referencia al vector y al activeRender correctos segun el contexto.

**Funciones a modificar (~30 funciones):**

| Funcion | Referencia actual | Reemplazar con |
|---------|------------------|----------------|
| `toggleCueByIndex` | `boxes.size()` | `getCueBoxSize()` |
| `setCueByIndex` | `boxes.size()`, `boxes[...]`, `*activerender` | helpers |
| `beginCueDraftForBoxIndex` | `boxes.size()`, `*activerender` | helpers |
| `buildCueDraftGraph` | `boxes.size()`, `boxes[...]`, iteracion | helpers |
| `cloneBoxForCueDraft` | `boxes[index]` | `getCueBox(index)` |
| `applyCueDraftToSource` | `boxes.size()`, `boxes[...]`, `*activerender` | helpers |
| `getCuePreviewBox` | `boxes[...]` | helper |
| `getCueDraftBoxForRealIndex` | usa draftRealIndices | OK (ya usa indices del draft) |
| `findCueDraftCloneIndexForRealIndex` | usa draftRealIndices | OK |
| `isCueSourceIndex` | usa sourceIndex | OK |
| Otras ~15 funciones | `boxes.size()`, `boxes[...]` | helpers |
| `clearCue` | varios | reset targetPreset |
| `getInspectorBox` | `boxes[...]` | OK (ya maneja group view) |
| Key handler (ofApp) | `toggleCueBoxByIndex(index)` | Pasar contexto |

**Helper methods a agregar:**
```cpp
vector<JPbox*>& getCueTargetBoxes();
int getCueTargetBoxSize() const;
JPbox* getCueTargetBoxAt(int index) const;
int& getCueTargetActiveRender();
int getCueTargetActiveRenderValue() const;
```

**Implementacion:**
1. Agregar `JPbox_preset *targetPreset = nullptr` a CueState en JPboxgroup.h
2. Agregar helpers en JPboxgroup.h y JPboxgroup.cpp
3. Reemplazar referencias en ~30 funciones
4. Modificar `toggleCueByIndex` y `setCueByIndex` para setear targetPreset
5. Modificar `clearCue` para resetear targetPreset
6. Actualizar el key handler de ofApp

