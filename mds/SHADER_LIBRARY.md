# Biblioteca pública de shaders

Estado: herramientas y navegador disponibles; selección de shaders pendiente.
No hay una colección aprobada ni nuevos shaders incorporados al AppImage.

## Archivos y compatibilidad

- `release/shader-candidates.json`: preselección local, no incluida en estos commits. Sus rutas, hashes y procedencia deben revisarse junto con los shaders antes de versionarla.
- `release/shader-library.json`: catálogo oficial versionado. Está vacío y `approved: false` hasta cerrar la selección.
- `scripts/shaders/`: inventario con el parser real, auditoría OpenGL aislada y generación de fichas.
- `scripts/release/shader_library.py`: barrera de empaquetado. Exige aprobación, 12/8/4 integrantes, traducciones, permisos revisados, evidencia local, hashes y dependencias presentes en la lista explícita.

La ruta identifica el shader; el nombre visible no cambia archivos, composiciones,
favoritos ni bindings MIDI. Los archivos sin catálogo conservan el acceso anterior.
El navegador lee el catálogo desde los recursos instalados y resuelve el shader desde
los datos del usuario. Si los bytes difieren de la copia instalada, lo identifica
como personal. AppPaths sigue protegiendo esas ediciones durante las actualizaciones.

## Usar IMPORT

1. Abrir IMPORT (atajo `4` cuando no se está escribiendo en otro campo). Al entrar, el buscador recibe el foco.
2. Escribir en Buscar. Los shaders actuales se encuentran por archivo o ruta; los catalogados también por nombre, descripción en español/inglés y etiquetas.
3. Abrir una carpeta y seleccionar un shader para ver la previsualización animada. Arriba/Abajo cambia la selección; la rueda desplaza la lista.
4. Usar CARGAR/LOAD, Enter o doble clic para agregarlo como nodo en la zona derecha. Un solo clic solo selecciona.
5. La estrella agrega o quita favoritos. El control superior derecho alterna cómo se muestran.
6. BIND inicia el aprendizaje MIDI para agregar ese shader; volver a pulsarlo cancela. EDIT abre su código en el editor.
7. El detalle muestra descripción y entradas, si hay catálogo; en los shaders sin catálogo muestra la ruta y las entradas detectadas.

Los clics dentro del panel, incluidos sus espacios vacíos y vista previa, quedan
reservados para IMPORT. Las cajas de atrás no reciben ese gesto, tampoco si se
arrastra fuera o se cambia de pantalla antes de soltar los botones.

Las categorías Generativos/Efectos/Mezcladores, nombres descriptivos y la etiqueta
Biblioteca oficial aparecen cuando se instala un catálogo aprobado. El catálogo
actual sigue vacío; por eso se mantienen las carpetas y nombres existentes.
Personal identifica archivos que difieren de los recursos instalados o no existen
en ellos. Biblioteca local indica que no hay una ficha oficial para ese recurso.
La categoría se deduce de `generative` (Generativos), `imageprocessing` (Efectos)
y `blending` (Mezcladores); los shaders catalogados usan su campo `category`.
No hay todavía un editor de categorías en IMPORT. Para la colección propuesta,
se edita `category` en `release/shader-candidates.json`; esa propuesta se activa
solo al aprobar e instalar el catálogo oficial, sin renombrar ni mover archivos.

La primera entrada de preview usa `guipper.png`; la segunda usa
`img/preview1.webp` de los recursos del usuario. Si falta, intenta la referencia
`image/demo-gradient.png` del paquete y, por compatibilidad, `preview2.png`.
La foto local no se añade automáticamente a la lista de distribución: su
procedencia debe documentarse al preparar los recursos de la próxima release.

## Reproducir inventario y fichas

Desde la raíz del proyecto, con el SDK fijado y un ejecutable Release actual.
Los comandos de auditoría y fichas requieren preparar primero el archivo local
`release/shader-candidates.json`; no viene incluido en un checkout limpio:

```sh
c++ -std=c++17 -O2 -Isrc/JPutils -I../../../libs/json/include scripts/shaders/parse-uniforms.cpp src/JPutils/jp_uniform_parser.cpp -o /tmp/guipper-uniform-inventory
python3 scripts/shaders/inventory.py --parser /tmp/guipper-uniform-inventory --output dist/shader-review/inventory.json
xvfb-run -a python3 scripts/shaders/audit.py --binary bin/Guipper --inventory dist/shader-review/inventory.json --candidates release/shader-candidates.json --output dist/shader-review/audit-new
python3 scripts/shaders/review.py --candidates release/shader-candidates.json --inventory dist/shader-review/inventory.json --audit dist/shader-review/audit-new --output dist/shader-review/review-new
```

Requiere Python 3, Pillow y Xvfb; las fichas usan DejaVu Sans del sistema.
El inventario no mueve ni elimina archivos. Registra uniforms, valores explícitos,
inclusiones, duplicados, atribuciones y commit de incorporación. La función se
clasifica por carpeta y tipo de entrada: es una clasificación inicial, no una
interpretación exhaustiva de cada algoritmo.

El lanzador copia ejecutable y recursos a una carpeta temporal, utiliza
`GUIPPER_USER_ROOT` aislado y ejecuta un proceso por shader con límite de 60 segundos.
No ejecutar los modos de regresión sobre el árbol de datos original.

Las capturas usan semilla 20260915, resolución 320×180 y 24 instantes a 6 fps.
Los efectos reciben un gradiente; los mezcladores reciben además un damero.
Se prueban extremos individuales de floats, ambos valores de booleanos y cuatro
semillas RDM. El informe registra compilación de nodo/vista previa, errores GL,
componentes no finitos y tiempos de GPU cuando existen. Las secuencias no certifican
automatización completa, combinaciones de extremos, audio ni feedback acumulado.

## Revisión y selección

1. Abrir `dist/shader-review/review/REVIEW.md` y las tres hojas de capturas. Revisar también GIFs y variantes RDM en los informes de auditoría.
2. Resolver salidas negras o poco representativas antes de elegirlas. No rellenar la cuota con candidatos pendientes de corrección.
3. Confirmar autoría y permiso por archivo. El autor de un commit no demuestra autoría del shader; la licencia global tampoco resuelve posibles incorporaciones externas.
4. Documentar evidencia de permisos en un archivo del repositorio, referenciado por `license.source`; completar autor y revisión. Si faltan candidatos válidos, informar el faltante.
5. Tras aprobación explícita, copiar solo los 24 elegidos al catálogo oficial, actualizar `release/assets.json` con sus hashes y dependencias y ampliar los créditos en `release/THIRD_PARTY.md`.
6. Verificar automatización, revisión animada, búsquedas, categorías, favoritos y copias personales con la colección final. Repetir regresiones y staging limpio; después reconstruir y probar AppImage.

El catálogo contiene `format`, `approved`, `entries`. Cada entrada conserva `path`,
`sha256`, `category`, `name` y `description` con `en`/`es`, `tags`, `inputs`, `author`
y `license` (`spdx`, `source`, `reviewed`). Las categorías son `generative`, `effects`
y `mixers`. Los nombres del catálogo no alteran nombres de uniforms.

## Validación de estos commits

- Pasan las 14 suites C++ (`make -C tests run`).
- Pasan las 21 pruebas Python de publicación (`python3 -m unittest discover -s tests -p 'release_*_tests.py'`).
- Los inventarios, capturas y resultados de auditorías anteriores sobre recursos locales no certifican la colección versionada: los cambios `.frag`, las composiciones `.xml` y el catálogo de candidatos quedaron fuera de estos commits.
- Pendiente: selección, verificación de hashes y permisos por archivo, revisión animada, pruebas de interacción en IMPORT y AppImage con la colección final.

Para las pruebas C++ independientes del SDK, instalar `nlohmann-json3-dev` o pasar
`JSON_INCLUDE` a Make. El workflow de pruebas instala esa dependencia.

Publicar requiere posteriormente la instrucción «publicá la próxima beta».
