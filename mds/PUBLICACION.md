# Publicación y actualizaciones

Estado al 2026-09-13: implementación en desarrollo, **no una versión pública certificada**.
No se publicaron releases, feeds, sitio ni invitaciones. No hay claves privadas en el repo.

## Datos del usuario

`jp::AppPaths` se inicializa antes de la aplicación. En Windows usa
`%LOCALAPPDATA%/Guipper/{data,config,state,cache}`. En Linux respeta
`XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `XDG_STATE_HOME` y `XDG_CACHE_HOME`, con
los valores predeterminados de XDG. `GUIPPER_USER_ROOT` permite perfiles aislados
para pruebas; no debe configurarse a una instalación ni compartirse entre versiones
incompatibles.

Los recursos instalados permanecen intactos. Para mantener las rutas relativas de
composiciones antiguas, el programa materializa una vista de trabajo en `data`.
Las copias oficiales tienen una referencia exacta en `cache/resource-baseline`:
una actualización solo las reemplaza si sus bytes coinciden con la referencia.
El editor retira esa referencia al guardar una copia personal. Eliminar el cache
conserva las copias existentes; nunca autoriza sobrescribirlas.

Una instalación antigua sin `distribution.marker` importa sus datos una vez,
sin reemplazar destinos existentes ni seguir symlinks. Los ajustes conocidos se
copian a `config`. Los archivos originales quedan intactos. Para una ubicación antigua diferente, `GUIPPER_LEGACY_DATA` permite indicar su
carpeta antes del primer inicio; el servicio `importLegacy()` conserva destinos
existentes. Todavía falta el selector gráfico de esa carpeta. La migración
automática cubre la carpeta de recursos con la que se inicia el programa. Los recursos oficiales retirados se conservan
en la vista materializada para no romper referencias anteriores.

Las rutas absolutas guardadas dentro de la carpeta de recursos antigua se
normalizan al perfil cuando pasan por `jp_normalizePath`. Los medios externos
siguen referenciados en su ubicación original.

## Persistencia y recuperación

- Escrituras temporales en el mismo filesystem y reemplazo atómico; flush antes
  del reemplazo. Los fallos de guardado se propagan al usuario y Save As permanece abierto.
- Proyectos y presets escriben `guipper_format=1`. Se conserva `.pre-v1.bak`
  antes de reemplazar un archivo antiguo. Las nuevas versiones rechazan formatos
  desconocidos; esto no puede corregir retroactivamente binarios ya distribuidos.
- La carga valida grupos recursivos, ciclos, versión y fuentes; construye nodos y
  enlaces candidatos antes de liberar el gráfico actual. Los tipos opcionales no
  disponibles mantienen la política histórica de omisión con advertencia.
- La recarga de shaders usa una sola instantánea de fuente para compilación y
  uniforms, y verifica `GL_LINK_STATUS`: `ofShader::isLoaded()` por sí solo no prueba
  que un programa haya enlazado correctamente.
- Cada 120 segundos se captura una instantánea si su contenido cambió. La
  serialización ocurre en el hilo principal; las escrituras ocurren en un worker
  que solo recibe bytes. Los grupos se capturan en archivos propios por UID.
- La recuperación se publica mediante un marcador después de escribir todo el
  conjunto. F9 abre una recuperación pendiente; antes guarda la composición actual
  en `savefiles/before-recovery.xml`. F8 descarta el aviso. Un guardado explícito exitoso retira el marcador; un cierre normal
  conserva una instantánea que todavía no fue guardada. Las generaciones se conservan; falta una política de retención.

**Límites vigentes:** el guardado de un proyecto con varios presets es atómico por
archivo, no una transacción indivisible de todo el conjunto. La carga de medios
asíncronos puede reportar un problema después de aceptar el gráfico. La captura de
PAINT/gráficos muy grandes todavía requiere medición de tiempos de serialización.

## Actualizaciones

F10 abre versión, consulta diaria opcional, canal, descarga, instalación y
exportación de diagnóstico. Las preferencias se guardan en `config/updates.json`.
El servicio limita las consultas automáticas a una por día y nunca descarga ni
instala sin acción explícita. Instalar exige guardar la composición y resolver
las pestañas modificadas del editor.

Los builds sin `jp_update_config.h` usan un adaptador desactivado: no consultan
Internet ni instalan nada. `scripts/release/configure_updates.py` genera únicamente
configuración pública, ignorada por Git. No ejecutar esa configuración hasta tener
SDKs nativos, feeds correctos, firmas y paquetes probados.

- Linux: adaptador para `libappimageupdate`, descarga sin sobrescribir y exige
  `VALIDATION_PASSED` (incluye rechazo de firmas ausentes/cambiadas). El helper
  espera el cierre del proceso, conserva la versión anterior y arranca la nueva.
  Si termina antes de informar arranque completo, restaura la anterior. Si queda
  colgada, no mata el renderer: requiere recuperación manual.
- Windows: adaptador condicional para WinSparkle, clave pública Ed25519, interfaz
  nativa de descarga y captura del instalador verificado. Solo la acción final de
  Guipper ejecuta el instalador. Inno Setup instala versiones lado a lado.
- **Pendiente:** compilación/enlace y validación real de ambos SDKs en paquetes
  firmados, progreso detallado/omisión por versión en Linux, interrupción inmediata
  de la conexión durante una consulta y rollback automático equivalente en Windows. La configuración beta
  de AppImage debe apuntar a un feed dedicado; no usar `latest` para ambos canales.

El diagnóstico exportado contiene versión, plataforma, GPU, canal y un registro
acotado de códigos de eventos. No incluye fuente GLSL, contenido de proyectos,
medios ni logs de compilador sin filtrar. Nada se envía automáticamente.

## Builds y paquetes

- `make -C tests run`: suites independientes, incluidos AppPaths y política de updates.
- `python3 -m unittest discover -s tests -p 'release_*_tests.py'`: allowlist del paquete.
- `python3 scripts/release/verify_dependencies.py --of-root ../../..`: verifica OF
  0.12.1 y el árbol de addons instalado. Es un lock del SDK preparado; no un downloader.
- `python3 scripts/release/stage.py --binary bin/Guipper --output /tmp/guipper-stage`:
  staging nuevo, recursos listados y verificados, ejemplos originales y licencias.
- Linux: `bash scripts/release/package-linux.sh`, con `LINUXDEPLOY`, `APPIMAGETOOL` y
  `APPIMAGE_RUNTIME` apuntando a las herramientas fijadas en `release/tools.lock.json`.
  El AppImage local es un candidato sin firma; no debe publicarse como estable.
- Windows: `_build_release_x64.bat` invoca el script PowerShell sin rutas personales.
  `release/guipper.iss` recibe `StageDir` y `AppVersion`; requiere Inno Setup.
  **Pendiente:** inventario explícito de DLLs/runtime y sus avisos para el paquete Windows.

Los workflows de release usan runners dedicados con `GUIPPER_OF_SDK`; copian el
SDK a un workspace aislado y construyen allí. Linux requiere las tres herramientas;
Windows requiere `ISCC`. Los jobs no aceptan PRs en esos runners. El resultado es
siempre un borrador. La firma del instalador Windows (Authenticode) es independiente
de la firma Ed25519 del payload de actualización.

`feed.py` genera un appcast local desde metadatos de un artefacto ya firmado;
valida estructura, no sustituye la verificación criptográfica del artefacto.
Los paquetes deben estar disponibles antes de publicar los feeds. No hay un job
que promueva automáticamente una release a estable.

## Lanzamiento pendiente de evidencia externa

Ver `release/BETA.md`, `release/START_HERE_ES.md`, `release/START_HERE_EN.md` y
`release/PROMOTION.md`. Incluyen recorrido, tres composiciones generadas y guiones
para un video de 75 s y tres demostraciones. Todavía deben grabarse con la app y
hardware real, actualizarse el sitio de descarga y agregarse el enlace de aportes.
Faltan pruebas Windows, dos horas por plataforma, instalación sin SDK y cinco
testers completando el recorrido. No se afirma que estas pruebas hayan ocurrido.

## Evidencia local de esta implementación

- Compilación Release Linux completada; 12 suites independientes aprobadas.
- Regresiones OpenGL de persistencia aprobadas, incluidos carga incompatible,
  recarga GLSL fallida y recuperación. AppPaths y política de actualizaciones
  también pasaron AddressSanitizer/UndefinedBehaviorSanitizer.
- Tres pruebas Python aprobadas: staging por allowlist, actualización del helper
  con respaldo y restauración cuando falla el primer arranque.
- AppImage generado en `dist/Guipper-0.1.0-beta.1-linux-x64.AppImage`.
  Arranque durante 15 segundos bajo Xvfb con perfil vacío y extracción del runtime:
  ejemplo inicial y audio inicializados. Es una prueba de arranque local, no una
  instalación limpia en otra máquina ni una sesión de dos horas. NDI no se validó.
- El AppRun reemplaza explícitamente el enlace creado por linuxdeploy antes de
  escribir el lanzador; conserva la selección de GPU del launcher existente.

El candidato local no está firmado y sus actualizaciones están desactivadas.

### Cancelación del adaptador Linux

La cancelación conserva el estado ocupado hasta que termina el trabajo del SDK.
Durante una descarga, solicita `stop()` una sola vez y espera `isDone()` antes
de admitir otra consulta. Si se cancela una consulta, su resultado se descarta
cuando termina: el SDK no ofrece una operación para abortar esa consulta de red.
Una finalización que coincide con cancelar nunca habilita la instalación.

La suite `appimage_backend_tests` ejecuta el adaptador condicional con un SDK
simulado y controla esas carreras, además de comprobar el rechazo de los estados
sin firma/firma inválida. No equivale a verificar criptografía con paquetes reales.
El adaptador también se comprueba sintácticamente contra el header upstream.
