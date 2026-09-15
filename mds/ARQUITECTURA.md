# Límites de arquitectura

Revisión del código local: **2026-09-12**.

Esta extracción separa operaciones y elimina duplicación de parámetros y tipos
de nodo. Las clases propietarias del grafo y las ventanas conservan sus APIs;
no cambia el formato de los proyectos ni migra su contenido.

## Dónde modificar cada comportamiento

| Responsabilidad | Implementación | Dependencias y límites |
| --- | --- | --- |
| Cargar/guardar composición, validar XML y reparar conexiones | `src/JPbox/JPboxgroup_persistence.cpp` | Métodos de `JPboxgroup`; conserva el control del ciclo de vida del grafo. |
| Campos XML de parámetros | `src/JPutils/jp_parameter_xml.*` | Solo XML y `JPParameterGroup`; no conoce ventanas, nodos, cue ni inspector. |
| Detectar y construir tipos de nodo | `src/JPbox/jp_box_factory.*` | No llama a `setup`, no conecta ni añade nodos al grafo. |
| Crear, aplicar, cancelar y sincronizar cue | `src/JPbox/JPboxgroup_cue.cpp` | Métodos de `JPboxgroup`; comparte las reglas de construcción con carga y pegado. |
| Ventanas de salida y sus callbacks | `src/ofApp_outputs.cpp` | Métodos de `ofApp`; controles de settings y XML permanecen en `ofApp.cpp`. |
| Ediciones reversibles del grafo | `src/JPbox/JPboxgroup_history.cpp` | Conserva la propiedad de nodos retirados cuando la operación es reversible. |

Linux/macOS descubren los archivos fuente mediante el Makefile de openFrameworks.
Los nuevos archivos también están registrados en `guipper.vcxproj` y sus filtros.
El registro de Windows no sustituye una compilación en Windows.

## Compatibilidad que debe conservarse

`jp_parameter_xml::load` exige un contexto explícito:

- `Composition`: encuentra parámetros por nombre, con posición como respaldo;
  restaura el valor actual y el suavizado.
- `Preset`: mantiene carga posicional y la restauración histórica del valor
  suavizado. No agrega la asignación del valor actual que esa ruta no hacía.
- `Clipboard`: mantiene carga posicional y restaura ambos valores.

Los campos opcionales mantienen valores de constructor cuando faltan. Los setters
existentes siguen aplicando rangos y validación. El guardado de las tres rutas
usa el mismo conjunto de campos; el orden de campos dentro de un parámetro puede
normalizarse, pero no cambia el orden de los parámetros.

La fábrica conserva dos órdenes históricos de reconocimiento: `Interactive`
para altas/pegado/cue y `Stored` para XML principal y presets. Por ejemplo,
`cam.xml` tiene interpretaciones diferentes en el código anterior. Esta
refactorización conserva esa diferencia de forma explícita; corregirla requiere
una decisión de compatibilidad y sus pruebas. Las condiciones NDI/Spout y la
prioridad de `camdepth` frente a `cam` se definen en un solo lugar.

## Propiedad y referencias

- La fábrica entrega un nodo sin inicializar. Quien llama asume su propiedad,
  ejecuta `setup` y lo transfiere al grafo o al borrador correspondiente.
- Los vectores del grafo todavía almacenan punteros crudos. La destrucción debe
  ejecutar `clear()` antes de `delete`, especialmente para presets anidados.
- Las conexiones de texturas, el inspector y las salidas toman referencias;
  no son propietarios de los nodos.
- Cue puede tomar referencias a FBOs vivos. El historial puede conservar nodos
  separados del grafo. Antes de migrar a `unique_ptr`, hay que representar esas
  transferencias, no reemplazar indiscriminadamente los tipos de los vectores.
- El módulo de parámetros toma referencias prestadas durante cada llamada y
  no las conserva.

## Verificación

Desde la raíz:

```bash
make Release -j2
make -C tests run
```

Desde `bin`, con un contexto gráfico disponible:

```bash
GUIPPER_PERSISTENCE_TEST=architecture ./Guipper
GUIPPER_PERSISTENCE_TEST=1 ./Guipper
```

El modo `architecture` ejecuta la carga segura, los contratos de parámetros y
la fábrica sin abrir dispositivos desde los casos de fábrica. La aplicación
sí realiza su inicialización habitual antes de las pruebas. La suite completa
además cubre cue, grupos, historial, conexiones, salidas y renderizado.
Ejecutar las pruebas sobre una copia de `bin/data` evita escribir fixtures en
los datos de trabajo.

Resultado de esta extracción: compilación Release en Linux, nueve suites del
núcleo, pruebas de límites `architecture` y los 69 indicadores de la suite gráfica
completa aprobados. La suite completa pasó antes y después bajo Xvfb. Los cuerpos
de cue y callbacks de salida se compararon con los originales: solo cambiaron las
llamadas a la fábrica en cue. Windows quedó registrado, pero no compilado aquí.

## Próximos pasos

Separar el estado de cue y salidas de sus clases anfitrionas y modelar las
transferencias de propiedad. La construcción transaccional de composiciones y
el guardado seguro siguen pendientes del punto 1; mover la implementación a
otro archivo no completa esas garantías.
