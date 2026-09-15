# Actualizaciones de Guipper en Linux

## Comportamiento

F10 permite consultar, descargar, cancelar, omitir una versión, abrir las
novedades del canal y guardar e instalar. La consulta diaria está desactivada
hasta que el usuario la activa. Una consulta automática nunca descarga.
«Buscar actualizaciones» permite volver a ofrecer una versión omitida.

La instalación guarda la composición y exige guardar primero las pestañas de
shaders. Mientras verifica de nuevo la firma, el panel bloquea la edición y
permite cancelar. Guipper cierra únicamente cuando el instalador externo pudo
iniciarse. El instalador espera a que termine el proceso, conserva `.previous`
y restaura ese ejecutable si el nuevo proceso termina sin confirmar su arranque.
Un proceso que sigue abierto no se mata para forzar una restauración.

El SDK se ejecuta en `guipper-update-worker`, fuera del render. Cancelar termina
su grupo de procesos y elimina la descarga temporal. Las consultas y la
verificación final tienen un límite de 60 segundos; las descargas, dos horas.
Los archivos temporales se crean con permisos privados junto al AppImage, en el
mismo sistema de archivos. Hace falta poder escribir en esa carpeta. Proyectos,
favoritos, mappings y shaders personales no forman parte del reemplazo.

## SDK y dependencias

Base de compilación: Ubuntu 24.04 x64, C++17, CMake, Git y estos paquetes de
desarrollo (además de los necesarios para Guipper):

```sh
sudo apt install cmake pkg-config libcurl4-openssl-dev libgcrypt20-dev \
  libgpg-error-dev libgpgme-dev libx11-dev libxpm-dev nlohmann-json3-dev gnupg
python3 scripts/release/build-linux-update-sdk.py \
  --work /ruta/temporal/update-build --output /ruta/sdk/guipper-updates
```

Los cinco repositorios están fijados por commit en `linux-update-sdk.json`.
Se usa libcurl del sistema con su biblioteca criptográfica actual. Se incluyen
las licencias del SDK y `UPDATE-SOURCE.tar.gz` con sus fuentes exactas, las
adaptaciones aplicadas y el código del worker; no se descarga una biblioteca
binaria opaca durante el arranque. Para verificar firmas se requiere GnuPG 2.2 o
posterior en el equipo; si falta, se rechaza la actualización y la app sigue
funcionando. Esto debe indicarse en los requisitos de descarga.

Adaptaciones pequeñas y explícitas al código fijado, aplicadas durante el build:

- Error recuperable si GPGME no encuentra un motor GnuPG.
- Exponer el nombre de destino después de consultar, para mostrarlo y omitirlo.
- Consultar con un destino separado del original; comparar los bytes instalados
  mediante un enlace temporal permite reconocer un AppImage renombrado.

El método `stop()` del SDK fijado no está implementado; no se utiliza.
`zsyncmake2` de esa revisión trunca el último bloque parcial: no se utiliza.
El `.zsync` lo genera la versión incluida en el `appimagetool` fijado y se
comprueban su longitud, SHA-1 completo, nombre y URL. SHA-1 pertenece al protocolo
zsync: la autenticidad se comprueba por separado con la firma OpenPGP.

Fuentes: [AppImageUpdate](https://github.com/AppImageCommunity/AppImageUpdate/tree/a211784dfc746fdb6d8d32d6bb39add451c1ddeb),
[appimagetool](https://github.com/AppImage/appimagetool).

## Clave y compilación firmada

La clave privada de publicación debe quedar bajo control del mantenedor, fuera
del repositorio y con respaldo. No usar las claves descartables de los tests.
Usar siempre la misma clave de firma para A y B; un cambio de clave se rechaza.
La huella requerida es la del firmante (40 caracteres), no un identificador corto.
La firma de AppImage es independiente de la firma del instalador Windows.

Ejemplo de configuración, sustituyendo propietario, repositorio y huella reales:

```sh
export GUIPPER_UPDATE_SDK=/ruta/sdk/guipper-updates
export GUIPPER_SIGNING_KEY=HUELLA_COMPLETA_DE_40_CARACTERES
stable='gh-releases-zsync|OWNER|REPO|stable|Guipper-linux-x64.AppImage.zsync'
beta='gh-releases-zsync|OWNER|REPO|beta|Guipper-linux-x64.AppImage.zsync'
python3 scripts/release/configure_updates.py --platform linux \
  --stable "$stable" --beta "$beta" --signing-fingerprint "$GUIPPER_SIGNING_KEY"
make Release -j2
export GUIPPER_UPDATE_INFORMATION="$beta"
export GUIPPER_DOWNLOAD_URL="https://github.com/OWNER/REPO/releases/download/v$(cat VERSION)/Guipper-$(cat VERSION)-linux-x64.AppImage"
bash scripts/release/package-linux.sh
```

También se requieren `LINUXDEPLOY`, `APPIMAGETOOL` y `APPIMAGE_RUNTIME`, del mismo
directorio verificado por `verify_tools.py`. El directorio de staging y el destino
firmado deben ser nuevos: no se reemplazan releases firmadas existentes.
`GNUPGHOME` y el agente GPG seleccionan la clave. No poner claves privadas ni
contraseñas en argumentos, archivos del proyecto o logs.

El paquete incluye el worker y sus dependencias. El empaquetador rechaza una
configuración incoherente, un binario sin el adaptador, una firma inválida o
metadata que no corresponda al archivo completo. Produce AppImage, `.zsync` y
recibo JSON con hash, huella, URL y revisiones del SDK.

Para volver a una compilación de desarrollo, quitar únicamente el archivo
GENERADO `src/JPutils/jp_update_config.h` y recompilar. Las actualizaciones estarán
desactivadas; el perfil del usuario permanece intacto.

## Publicación: paquetes primero, canal después

El workflow genera una release en borrador; no mueve los canales. Requiere la
variable pública de repositorio `GUIPPER_LINUX_SIGNING_FINGERPRINT`, la clave ya
provisionada en el runner protegido y los paquetes de desarrollo anteriores.
Los tests usan su propio GNUPGHOME temporal y no acceden a esa clave.

1. Probar y publicar la release `vVERSION` con el AppImage firmado y su `.zsync`.
2. Confirmar que la URL inmutable del AppImage ya se puede descargar.
3. Copiar el `.zsync` validado a `Guipper-linux-x64.AppImage.zsync`, conservando el
   contenido: su URL debe seguir apuntando al AppImage de `vVERSION`.
4. Publicar ese archivo y las novedades en la release de canal `beta` o `stable`.
   Cada canal debe tener un único archivo con ese nombre. La actualización del
   asset puede dejar un intervalo breve sin feed: Guipper muestra un error y
   permite volver a consultar; no instala un paquete incompleto.
5. Comprobar desde A la actualización a B con un perfil de prueba.

Una etiqueta o commit por sí solo no actualiza instalaciones existentes.
La clave pública de publicación está en `keys/guipper-updates-public.asc`; su
huella es `EAE485030F022D467FCFD36E0F659D1854076536`. Los canales previstos de
`guipper/Guipper` están registrados en `linux-update-channels.json`. La clave
privada permanece en el llavero GPG del mantenedor. Publicar los canales sigue
siendo un paso separado.

## Validación

```sh
make -C tests run
python3 -m unittest discover -s tests -p 'release_*_tests.py'
python3 scripts/release/test-linux-updates.py --sdk "$GUIPPER_UPDATE_SDK" \
  --tool "$APPIMAGETOOL" --runtime "$APPIMAGE_RUNTIME"
```

La última prueba genera AppImages ejecutables pequeños, utiliza el worker real y
un servidor HTTP de loopback, y conserva evidencia en `/tmp/guipper-native-updates-*`.
El HTTP local existe solo en el harness; el configurador de distribución exige
HTTPS o GitHub. Las claves temporales se eliminan al terminar, incluso si falla.
Se comprueban A → B, firma cambiada, ausencia de firma, bytes alterados, cancelación,
falta de red, reconocimiento de la versión ya instalada aunque esté renombrada,
restauración tras un fallo de arranque y conservación de archivos del perfil.

Esto no sustituye el recorrido manual con el AppImage completo en hardware real,
la publicación del canal, un ensayo de disco lleno o una actuación de dos horas.

## Registro local — 15 de septiembre de 2026

- 13 suites C++ y 17 tests Python aprobados.
- Nueve casos nativos aprobados, incluida cancelación con transferencia en curso.
- Regresiones OpenGL aprobadas con recursos y ejecutable copiados a un directorio
  temporal. El conjunto mínimo de distribución no contiene todos los fixtures
  de desarrollo: para esta suite se copió `bin/data` completo.
- Panel inspeccionado a 1080×750 y 420×320 con un worker controlado: progreso,
  omisión persistida, desplazamiento y Escape.
- AppImage completo firmado construido y arrancado; dependencias del worker
  resueltas desde el paquete. Una consulta nativa a un feed deliberadamente
  inexistente mostró el error esperado y permitió cerrar normalmente.
- Claves de prueba eliminadas. Configuración de prueba retirada y compilación
  de desarrollo reconstruida con actualizaciones desactivadas.

Evidencia local: `dist/linux-updates-validation-20260915/report.json`, capturas y
logs en la misma carpeta. No se publicó ningún paquete ni se modificó un canal.
El AppImage firmado de ensayo usa una clave descartada y no es distribuible como
release pública. El recorrido A → B nativo utiliza ejecutables pequeños; falta
repetirlo con Guipper completo en el equipo del mantenedor y el canal público.
