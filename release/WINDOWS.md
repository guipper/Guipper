# Windows x64: preparación del runtime

Estado al 2026-09-18: beta.6 compilada, empaquetada y ejecutada en Windows.
El manifiesto `windows-runtime.json` contiene DLLs y avisos revisados con SHA256.
Ver [resultados y límites](../mds/PRUEBA_BETA6_WINDOWS_2026-09-18.md).
NDI es opcional y requiere instalar el runtime NDI 6 por separado; no se redistribuye.

## Actualizaciones Windows

Usar WinSparkle 0.9.4, cuyo archivo oficial y SHA256 figuran en
`windows-update-sdk.json`. Configurar `GUIPPER_WINSPARKLE_SDK` con la carpeta que
contiene `include` y `x64`; `build-windows.ps1` genera la configuración pública y
enlaza el SDK. `-Output` permite compilar sin reemplazar una aplicación abierta.
El canal beta usa el release `windows-beta`; stable usa `windows-stable` y permanece
vacío hasta disponer de una versión estable. Linux conserva sus propios canales.
Las instalaciones nuevas beta seleccionan beta; una preferencia existente se respeta.

Firmar cada instalador con `winsparkle-tool sign --private-key-file CLAVE INSTALADOR`.
La clave privada se guarda fuera del repositorio y debe respaldarse de forma segura.
La clave pública está en `keys/guipper-windows-ed25519.pub`. Generar el appcast con
`scripts/release/feed.py`, publicar primero el artefacto versionado, verificar su
descarga y recién entonces publicar el appcast. No reemplazar un instalador publicado.

`test-windows-updates.py` usa el backend real y WinSparkle con un servidor local;
requiere el probe CMake, instalador, herramienta de firma y clave privada.
`test-windows-install.py` recibe el instalador verificado, ejecutable anterior,
staging y carpeta de salida nueva para comprobar instalación y conservación de datos.

## Preparación en Windows 11

1. Instalar Visual Studio 2022 con C++ x64, Python e Inno Setup. Preparar el SDK
   openFrameworks 0.12.1 y los addons coincidentes con el lock. El SDK Linux no
   contiene las bibliotecas de openFrameworks necesarias para enlazar Windows.
2. Ejecutar `scripts/release/build-windows.ps1` desde PowerShell y comprobar
   `bin/Guipper.exe`. Usar las herramientas de esa instalación de Visual Studio.
3. Obtener las dependencias con `dumpbin /dependents bin/Guipper.exe` y repetir
   para cada DLL. Agregar también las bibliotecas cargadas dinámicamente, como
   NDI, y plugins de medios; dumpbin no puede descubrir esas cargas por nombre.
4. Preparar una carpeta externa con las DLLs redistribuibles x64 y sus avisos de
   licencia. No copiar DLLs desde System32. Para mantener instalación por usuario,
   usar el runtime MSVC app-local que corresponda al compilador, proveniente del
   directorio de redistribución de Visual Studio y sujeto a sus términos.
5. Completar `release/windows-runtime.json` con la versión exacta del SDK,
   `review_note`, una entrada por DLL y `reviewed: true` solo después de revisar
   procedencia, permisos de redistribución y cargas dinámicas.

Cada entrada tiene esta estructura (los valores ilustrativos NO son artefactos):

```json
{
  "path": "relative/component.dll",
  "sha256": "64 caracteres hexadecimales del archivo real",
  "source_url": "https://sitio-oficial-del-proveedor/",
  "license": "identificador o nombre de licencia",
  "notice": "licenses/component.txt",
  "notice_sha256": "64 caracteres hexadecimales del aviso real"
}
```

Las rutas parten de `GUIPPER_WINDOWS_RUNTIME_ROOT`; el paquete aplana las DLLs
junto a Guipper.exe. No se admiten nombres duplicados, rutas externas, archivos
x86 ni hashes distintos. Los imports normales y diferidos se contrastan contra
las DLLs incluidas y una lista explícita de componentes de Windows 11; un import
desconocido detiene el paquete, no se clasifica automáticamente como sistema.
Los contratos `api-ms-win-*` y `ext-ms-win-*` se resuelven por Windows.

```powershell
$env:GUIPPER_WINDOWS_RUNTIME_ROOT = 'C:\GuipperRuntime'
$env:ISCC = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
./scripts/release/package-windows.ps1
```

Elegir otro `-Output` si el staging ya existe. El script produce el staging por
allowlist, copia solo las DLLs revisadas, genera un recibo interno y llama a Inno.
El recibo evita usar accidentalmente el staging genérico anterior; no es una firma
criptográfica ni reemplaza la revisión de los archivos. El workflow usa este mismo
script y requiere configurar ambas variables en el runner Windows dedicado.

## Aceptación antes de distribuir

Probar en Windows 11 sin Visual Studio ni SDK: instalación sin administrador,
primer inicio, tres ejemplos, audio/MIDI/salida, guardado y recuperación. Probar
medios y NDI explícitamente si se van a anunciar, porque las importaciones estáticas
no cubren plugins ni bibliotecas cargadas dinámicamente. No publicar el borrador
hasta completar los resultados de `BETA.md`.

La instalación usa directorios por versión. No elimina `%LOCALAPPDATA%\Guipper`
al desinstalar y no cierra una instancia activa. La firma
de actualizaciones Ed25519 está implementada; Authenticode y la restauración
automática de Windows siguen pendientes. La recuperación probada es manual,
ejecutando la versión anterior que permanece disponible.

Referencias: [redistribución de Visual C++](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170),
[dependencias que redistribuir](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute?view=msvc-170),
[instalación sin elevación de Inno Setup](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm).

## Curador desde el repositorio

Para colaborar con Linux sin generar un instalador, compilar y ejecutar
`run-curated.bat`. Ver [guía de curación compartida](../mds/CURADOR_COMPARTIDO.md).
