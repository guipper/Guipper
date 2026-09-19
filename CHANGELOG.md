# Changelog

Cambios visibles para quienes usan Guipper, separados del historial técnico de Git.
Cada cambio se anota primero en **Sin publicar** y se mueve a su versión al publicarla.
Las notas de GitHub Releases deben resumir la misma entrada. Las pruebas y sus
límites se registran por separado en `mds/PRUEBA_*.md`.

## Sin publicar

- Tres transiciones que toman los colores de ambas composiciones: Eco cromático, Datamosh de paleta y Glitch espectral, con memoria de cuadros, análisis de sombras/medios/luces en GPU y control de intensidad.

- Corregido un cierre al cambiar el shader activo con NDI habilitado: la salida conserva su resolución aunque cambie el tamaño de las imágenes durante una transición.

## [0.1.0-beta.5](https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.5) — 2026-09-18

### Novedades principales
- Nuevo catálogo de transiciones con familias, favoritos, aleatorio, sincronización al pulso y prueba aislada; panel plegable en SETTINGS.
- Transiciones entre composiciones completas, incluyendo FINAL, con correcciones de frames negros y continuidad al cargar XML.
- Tipografía Overpass y ajustes de alineación y filtrado para mejorar la legibilidad.
- Edición de texto unificada: selección, navegación por palabras, portapapeles y deshacer local; no incluye el editor de shaders.
- IMPORT responde mejor al abrir y buscar; abrir el panel ya no captura automáticamente el teclado en el buscador.
- CUE refleja los parámetros de imágenes y cámaras. Los rangos personalizados muestran su valor al arrastrar y la velocidad de automatización permite ajustes más finos.
- Carga por arrastre de composiciones con rutas antiguas o relativas corregida.
- Revisión compartida del curador mediante carpeta sincronizada, comentarios e historial de cambios.


### Correcciones
- Linux excluye correctamente el backend Core Audio de macOS durante la compilación.
- Gameboy Palette conserva la transparencia de entrada y la mezcla al ajustar la intensidad del efecto.
- FINAL conserva el alpha del fondo al superponer capas semitransparentes; el renderer ya no invalida los factores de mezcla separados.
- La preview de IMPORT usa imágenes de prueba solo en entradas de textura declaradas. El feedback interno queda separado, evitando que generadores sin inputs muestren accidentalmente la imagen de prueba.
- Cargar una sesión con `L` restaura los valores de los parámetros dentro de grupos, incluidos grupos anidados, sin conservar los valores aleatorios de inicialización.
- Los clics en IMPORT bloquean las cajas y controles que se ven detrás. El bloqueo se conserva hasta soltar los botones, incluso al cargar un shader o arrastrar fuera del panel.

### Mejoras
- Los filtros de curado incluyen «Mejorar» y «Pupper», combinables con búsqueda y carpeta.
- Curado permite marcar cada shader con «Mejorar shader» y «Preguntarle a Pupper», con marcas visibles en la lista y guardado inmediato.
- IMPORT unifica el encabezado del inspector, ajusta su altura al contenido y muestra pestañas con nombres cortos y cantidades. El curador experimenta en un borrador local y publica sus valores con «Guardar como default».
- LOAD importa los valores actuales de preview. Usuarios comunes pueden ajustar parámetros con RANDOM y RESET; RESET recupera los defaults guardados por el curador, sin sobrescribirlos.
- El inspector curado incluye RANDOM junto a RESET para aleatorizar y guardar los parámetros de la preview respetando sus rangos.
- IMPORT conserva la preview para todos los ítems, ajusta su recuadro a la imagen y agrega pestañas de carpetas. Curado permite filtrar por visibilidad para usuarios o por la marca de parámetros pendientes, combinado con la búsqueda.
- Curado incluye toda la biblioteca y permite marcar ítems para usuarios, anotar parámetros pendientes y asociar grupos XML con preview y carga conjunta. `run-user.sh` prueba solo la selección habilitada.
- IMPORT ordena por nombre visible en el idioma activo, también en Favoritos. En curado se puede editar el nombre desde el inspector; Enter o clic fuera guarda y Escape cancela.
- Avisos reutilizables abajo al centro para recuperación, guardado de composición y ajustes, errores y actualizaciones: hasta tres, cierre con ×, pausa al pasar el cursor y acciones de recuperación sin perder el snapshot al guardar.
- En curado, el shader seleccionado tiene un inspector de preview con sliders, booleanos, reset y ajustes guardados por shader en el perfil de prueba. Las filas de IMPORT son más compactas.
- IMPORT reúne detalles y vista previa en la zona inferior, mejora la legibilidad de la lista y agrega una barra de desplazamiento arrastrable, scroll de trackpad y navegación por páginas.
- IMPORT distingue biblioteca local de catálogo oficial y muestra la categoría derivada de la carpeta.
- La segunda entrada de la vista previa usa `img/preview1.webp`; la primera conserva el logo. Si la foto no está disponible, se usa la referencia incluida en el paquete.
- Navegador preparado para nombres, descripciones bilingües y etiquetas del catálogo oficial, conservando rutas y favoritos existentes.
- Descripción, entradas requeridas y distinción entre biblioteca oficial y copias personales en la selección.
- Vista previa enlazada a los nombres de las entradas declaradas y controles iniciales con valores definidos; navegador adaptado a ventanas pequeñas.
- Ejemplos de Getting Started accesibles en el navegador.

El paquete incluye los recursos internos y ejemplos autorizados. La biblioteca personal y las colecciones contribuidas no se redistribuyen; el catálogo oficial continúa vacío hasta completar la revisión de procedencia. La investigación de profundidad mediante webcam RGB queda pausada y no añade una función a esta beta. La publicación binaria de esta versión es para Linux; Windows/macOS requieren validación separada.

## [0.1.0-beta.4](https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.4) — 2026-09-15

### Correcciones
- GitHub Actions: rutas del SDK aislado definidas en los pasos de Linux y Windows,
  evitando el error de validación de `runner.temp` en `defaults.run`.

### Mejoras
- Panel F10 alineado con los botones, colores y tipografía compartidos de Guipper.
- Acciones de actualización, preferencias y soporte agrupadas; se destaca la
  acción disponible y se muestra el estado actual del canal y la consulta diaria.
- Barra de progreso, mensajes completos con desplazamiento y distribución
  adaptable a ventanas pequeñas. Cierre con botón Esc y navegación con
  Page Up, Page Down, Home y End; se mantienen los atajos 1–9.

## [0.1.0-beta.3](https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.3) — 2026-09-15

### Correcciones
- Combinar clic izquierdo y derecho, o hacer doble clic derecho, ya no aleatoriza
  los parámetros. La acción manual se activa con clic izquierdo en el botón RDM.

### Validación
- Actualización beta.2 → beta.3 comprobada contra el canal público.
- El usuario confirmó que la actualización y el fix funcionan en su equipo Linux.

## [0.1.0-beta.2](https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.2) — 2026-09-15

### Correcciones
- Alt izquierdo, Alt derecho y Ctrl+Alt ya no abren el debug avanzado.
  Ctrl+D mantiene su función.

### Distribución
- Primera pre-release publicada con AppImage Linux x64 firmado y canal beta activo.
- Actualización beta.1 firmada → beta.2 comprobada con respaldo de la versión anterior.

## 0.1.0-beta.1 — candidato local, no publicado

### Base de la beta
- Datos personales separados de los recursos instalados y migración compatible.
- Guardado seguro, recuperación, diagnóstico voluntario y ejemplos iniciales.
- Panel F10 y actualización Linux con descarga, verificación de firma,
  cancelación e instalación explícita.

Existieron compilaciones beta.1 sin actualizador y un candidato posterior firmado
con actualizaciones habilitadas. Solo este último permite actualizar desde F10.
