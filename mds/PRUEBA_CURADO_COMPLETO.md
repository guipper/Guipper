# Biblioteca completa y marcas de curado

- El catálogo coincide con los 461 `.frag` presentes en `bin/data/shaders`, sin duplicados; 30 habilitados inicialmente para usuarios.
- `make -C tests run`: suites puras, incluyendo validación de marcas, grupos y categorías del catálogo.
- Prueba nativa `curation`: descubre toda la biblioteca; persiste marcas independientes; excluye ítems desmarcados de la versión común sin filtraciones por el escaneo genérico; comparte nombres; rechaza grupos inexistentes y errores de escritura; previsualiza y carga un grupo con dos nodos sin reemplazar la composición abierta; quitar la asociación restaura el shader individual.
- Prueba nativa `curated_preview`: sliders, booleanos, persistencia, reset, scroll y captura del gesto con las filas de revisión presentes.
- Capturas aisladas en `dist/full-curation`: escritorio 1440×840 y ventanas 600×600 / 400×430. Clic real en «Add parameters» verificado contra una copia temporal del catálogo.
- Compilación Linux. Archivos nuevos registrados también en el proyecto Visual Studio; no se ejecutó compilación Windows.

La prueba del catálogo no compila cada shader experimental. Los grupos se asocian a XML existentes; sus dependencias deben estar disponibles. BIND permanece para shaders individuales, no para grupos asociados.


## Ajuste de preview y pestañas

La preview vuelve a mostrarse también en generadores sin inputs. Su alto se ajusta
al aspecto del FBO, sin las franjas vacías de centrado. Las pestañas seleccionan una
carpeta y se combinan con los filtros existentes. Pruebas nativas: `curation`
(aislamiento de categoría y preview de generador), `curated_preview` y `import_scroll`.
Capturas de escritorio y ventana pequeña: `dist/curation-tabs`.


## Imágenes de prueba y feedback

La prueba nativa `curated_preview` dibuja un shader que solo lee `feedback`, con una
imagen de prueba roja cargada: el resultado debe ser negro neutro. Otro shader con
entrada explícita debe recibir esa imagen roja sin contaminar el feedback. Volver
al primero debe seguir dando negro, sin heredar la textura de la selección anterior.
La miniatura de salida sigue visible para todos los shaders.


## Defaults compartidos para usuarios

- Suite `curation`: guarda defaults del curador en el catálogo; abre el modo usuario;
  confirma inspector sin controles de curación, RANDOM sin modificar el catálogo,
  LOAD con valores de preview y RESET a los defaults del curador.
- Suite `curated_preview`: importación de un booleano en falso, además de sliders,
  reset, scroll, persistencia y aislamiento entre inputs de prueba y feedback.
- Suite pura de catálogo: valores numéricos/booleanos aceptados y tipos inválidos rechazados.
- Capturas `dist/user-preview`: modo usuario real en escritorio y ventana pequeña.
