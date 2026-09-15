# Changelog

Cambios visibles para quienes usan Guipper, separados del historial técnico de Git.
Cada cambio se anota primero en **Sin publicar** y se mueve a su versión al publicarla.
Las notas de GitHub Releases deben resumir la misma entrada. Las pruebas y sus
límites se registran por separado en `mds/PRUEBA_*.md`.

## Sin publicar

### Correcciones
- Los clics en IMPORT bloquean las cajas y controles que se ven detrás. El bloqueo se conserva hasta soltar los botones, incluso al cargar un shader o arrastrar fuera del panel.

### Mejoras
- IMPORT distingue biblioteca local de catálogo oficial y muestra la categoría derivada de la carpeta.
- La segunda entrada de la vista previa usa `img/preview1.webp`; la primera conserva el logo. Si la foto no está disponible, se usa la referencia incluida en el paquete.
- Navegador preparado para nombres, descripciones bilingües y etiquetas del catálogo oficial, conservando rutas y favoritos existentes.
- Descripción, entradas requeridas y distinción entre biblioteca oficial y copias personales en la selección.
- Vista previa enlazada a los nombres de las entradas declaradas y controles iniciales con valores definidos; navegador adaptado a ventanas pequeñas.
- Ejemplos de Getting Started accesibles en el navegador.

La colección de 24 shaders sigue pendiente de selección visual y revisión de procedencia. La preselección permanece local; estos cambios no incluyen archivos `.frag` ni `.xml` nuevos o modificados. El catálogo oficial continúa vacío.

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
