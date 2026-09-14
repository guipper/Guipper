# Parser de uniforms

Implementación revisada el **2026-09-12**.

`jp_uniform_parser::parse(source)` inspecciona texto GLSL y devuelve declaraciones
y diagnósticos con línea/columna, sin depender de openFrameworks, OpenGL, archivos,
valores aleatorios ni del estado de la aplicación. Los nodos y la vista previa
comparten este parser; cada uno aplica sus filtros de globals.

## Sintaxis admitida

```glsl
  uniform highp
  float amount = -2.5e-2, speed = 0.5;
uniform bool enabled = true;
uniform sampler2D source;
uniform sampler2DRect previous;
uniform float red = 0.8; // @color r tint
uniform sampler2D privateMask; // @internal
```

Se admiten espacios/tabulaciones, CRLF, BOM UTF-8, comentarios de línea/bloque,
calificadores, declaraciones multilínea y listas separadas por comas. Se inspeccionan
las declaraciones globales, no texto dentro de funciones o comentarios. Las
anotaciones se toman de comentarios dentro de la declaración o inmediatamente
después de su punto y coma; no se aplican a otra declaración de esa misma línea.

Los tipos que generan controles son `float`, `bool`, `sampler2D` y `sampler2DRect`.
Los valores float deben ser literales decimales finitos; se admiten signos,
exponentes y sufijo `f/F`. El valor bool puede ser `true` o `false`.

## Límites y diagnósticos

El resultado distingue errores estructurales de advertencias:

- Declaraciones incompletas, nombres duplicados y comentarios sin cerrar son
  errores. El parser intenta recuperar las declaraciones siguientes para informar
  los problemas, pero el nodo no instala un resultado que contenga errores.
- Arrays, bloques y tipos sin control de inspector se informan y no crean controles.
- Las expresiones de inicialización no se evalúan. Se informa la limitación y el
  consumidor usa su fallback: float aleatorio en `[0,1)` o bool false.
- Los includes y macros no se expanden. Las declaraciones en condicionales se
  inspeccionan sin resolver las ramas y generan una advertencia. Si ambas ramas
  declaran el mismo nombre, se informa duplicación; no se elige una rama.
- No es un compilador GLSL. El compilador gráfico sigue validando el shader completo.

`JPbox_shader::uniformDiagnostics` conserva los diagnósticos y el log
`uniform-parser` incluye ruta, línea y columna. Los globals conocidos de tipos
no editables se conservan en el resultado sin repetir esas advertencias en el log.
No se agregó un panel nuevo de errores en el editor.

## Compatibilidad y recarga

Se mantienen el orden de los parámetros, los globals históricos y las extracciones
aleatorias que determinan los valores iniciales y semillas. Solo los globals
excluidos previamente por `isNewGlobalName` se omiten en los nodos; la vista previa
puede filtrar todos los globals porque no tiene posiciones persistidas.

La comparación de la biblioteca local abarcó **461 archivos**:

- 458 inventarios idénticos: nombres, orden, valores iniciales, tipos, entradas y colores.
- 3 diferencias limitadas a espacios sobrantes en nombres: `contrib/cubesaredancing.frag`,
  `contrib/day43.frag.frag` y `imageprocessing/vignette.frag`.

Los nombres antiguos con espacios se aceptan en la resolución de parámetros y
entradas. Los matches exactos tienen prioridad en carga; el respaldo normalizado
se intenta antes de recurrir a posición. No se modifican archivos XML ni shaders
para realizar esta compatibilidad.

La recarga realiza un solo intento. Un archivo vacío, ilegible o con errores
estructurales de uniforms conserva los controles, conexiones y programa anterior.
Una recarga válida restaura valores y configuración de audio por nombre/tipo,
actualiza las anotaciones y conserva conexiones por nombre. Un cambio de tipo usa
el nuevo valor inicial, sin leer campos del tipo anterior. Un shader válido sin
uniforms puede recargarse normalmente.

La recarga ahora compila un programa candidato y comprueba `GL_LINK_STATUS`
antes de reemplazar controles o programa. La compilación y la inspección de
uniforms reciben la misma instantánea de fuente. Ver [publicación](PUBLICACION.md).

## Pruebas reproducibles

```bash
make -C tests run
```

Incluye la suite independiente `uniform_parser_tests`, con sintaxis válida,
recuperación de errores y 6.000 entradas deterministas de estrés. Puede ejecutarse
con AddressSanitizer y UndefinedBehaviorSanitizer:

```bash
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  tests/uniform_parser_tests.cpp src/JPutils/jp_uniform_parser.cpp \
  -o /tmp/guipper-uniform-parser-sanitized
/tmp/guipper-uniform-parser-sanitized
```

Después de `make Release -j2`, desde una copia aislada de `bin` con sus datos:

```bash
GUIPPER_PERSISTENCE_TEST=uniform_parser ./Guipper
GUIPPER_PERSISTENCE_TEST=1 ./Guipper
GUIPPER_PERSISTENCE_TEST=uniform_inventory ./Guipper
```

El último modo genera `data/uishots/persistence/uniform_inventory.json`, usando la
misma semilla para cada shader. Guardá una copia antes y después de un cambio:

```bash
python3 tests/compare_uniform_inventory.py before.json after.json
```

El comparador distingue coincidencias exactas, normalizaciones de espacios y
cambios que requieren revisión; estos últimos producen código de salida 1.
