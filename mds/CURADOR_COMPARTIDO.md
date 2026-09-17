# Curador compartido: Nico y JPupper

## Preparar Windows

Usar el mismo checkout y fuentes de shaders en ambos equipos. En Windows instalar
Visual Studio 2022 con desarrollo de escritorio C++, SDK Windows, Python y
openFrameworks **0.12.1 para Visual Studio**. Ubicar Guipper en
`<OF>/apps/myApps/Guipper`. Preparar los addons de `addons.make` y las versiones
registradas en `release/dependencies.lock.json`, incluidas sus bibliotecas para
Windows x64. No copiar las bibliotecas del SDK Linux.

Desde PowerShell, en el repositorio:

```powershell
.\scripts\release\build-windows.ps1
.\run-curated.bat
```

El script existente verifica las dependencias y compila Release x64. También se
puede abrir `guipper.sln` en VS2022 con esa configuración. Las DLL necesarias
para ejecutar deben estar disponibles junto al ejecutable o según la configuración
del SDK; consultar [preparación Windows](../release/WINDOWS.md). No hace falta
crear un instalador ni usar Inno Setup para compilar y ejecutar desde el repo.

El launcher usa `release/shader-curated.json` y un perfil independiente en
`dist/curated-profile`; admite `GUIPPER_CURATED_LIST` y `GUIPPER_USER_ROOT` para
perfiles personalizados. En Linux usar `./run-curated.sh`.

**Windows sigue pendiente de compilación y ejecución en un equipo Windows.**
La compilación Linux no certifica compatibilidad binaria con Windows.

## Crear y abrir la revisión

1. Configurar fuera de Guipper una carpeta sincronizada entre los dos equipos.
2. Seleccionar un shader en IMPORT y abrir **Comentarios / revisión** en el
   inspector curador.
3. Nico: **Revisor**, escribir `Nico`; **Crear**, elegir una carpeta vacía. Se
   copia la curación actual como punto de partida.
4. Esperar a que la herramienta externa sincronice esa carpeta.
5. JPupper: **Revisor**, escribir `JPupper`; **Abrir**, elegir su copia de la misma
   carpeta. Abrir usa la revisión existente; no la reemplaza con su catálogo.

Cada equipo conserva su propio catálogo local, perfil y caché. No sincronizar el
perfil completo ni usar un mismo archivo de catálogo local para ambos procesos.
La carpeta compartida contiene `review-project.json` y `events/*.json`. No editar
ni borrar esos archivos: son el historial inmutable. La carpeta `.creating` es un
bloqueo de inicialización; si la creación se interrumpe, comprobar que ningún
proceso esté creando la revisión antes de retirarla y reintentar en una carpeta vacía.

## Qué se comparte

- Cada marca por separado y nombres visibles EN/ES por separado.
- **Guardar como default** publica todos los parámetros como un conjunto.
- Sliders y RANDOM guardan únicamente borradores locales. Los defaults resueltos
  se escriben en el catálogo local para el modo usuario; RESET del usuario usa
  esos defaults. Para recibir novedades hay que abrir el curador conectado.
- Comentarios con autor, fecha UTC y propuesta: comentar, conservar, modificar o
  eliminar. Una propuesta nunca borra un shader.
- Fuentes de shaders, asociaciones a grupos y composiciones permanecen locales.

El proyecto parte de los shaders presentes en la curación inicial. Un shader
incorporado después al catálogo local no pertenece automáticamente a esa revisión.

## Conflictos, conexión y comentarios

Los cambios se guardan primero en la cola local y luego como archivos separados
con IDs únicos. El curador revisa la carpeta cada dos segundos; **Actualizar**
solicita una lectura inmediata. Mientras actualiza, esperar antes de publicar
otro cambio. El estado indica lectura local, pendientes y errores de archivos;
no confirma recepción en el otro equipo.

Se combinan campos distintos y valores iguales. Para valores diferentes del mismo
campo, **Historial / conflictos** conserva el último valor común y permite **Usar
esta versión**. La elección queda registrada con autor y revisiones anteriores.
Dos resoluciones simultáneas distintas generan otro conflicto; no se pierde ninguna.

Si la carpeta desaparece, se usa la copia local y las publicaciones quedan en cola.
Los archivos incompletos se vuelven a leer; una revisión que llega antes que sus
antecesoras espera hasta recibirlas. Los comentarios publicados sobreviven a
reinicios. Para corregir un comentario, publicar otro. El borrador se conserva si
falla la escritura local. **Local** desactiva la colaboración sin borrar su caché.

El contador del inspector muestra comentarios, actividad no leída y conflictos.
La lista usa `!` para actividad pendiente; el detalle está en el tooltip. El
seguimiento de lectura es local y se separa por nombre del revisor y proyecto.
El editor admite varias líneas, Enter, Ctrl+A/C/V y Ctrl+Enter para publicar.

Comentarios y defaults incluyen una huella del archivo fuente. Si difiere de la
fuente local, aparece **Versión de shader diferente**. La huella identifica el
archivo del shader, no todas sus dependencias ni los contenidos de grupos.

## Validación reproducible

- `make -C tests run`: pruebas puras, incluidos dos perfiles, conflictos,
  resoluciones concurrentes, duplicados, recepción fuera de orden y desconexión.
- `GUIPPER_PERSISTENCE_TEST=shared_review`: integración nativa con proyecto y
  perfiles temporales: publicación, defaults, conflictos y botón de resolución.
  Ejecutar en una copia aislada de `bin` con un display/OpenGL disponible.
- Antes de dar Windows por validado: compilar, abrir `run-curated.bat`, conectar
  un segundo perfil, comentar en ambos, desconectar/reconectar y resolver un
  conflicto. Revisar ES/EN y ventanas pequeñas.
