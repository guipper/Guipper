# Windows x64: preparación del runtime

Estado: herramientas de staging implementadas y probadas con fixtures en Linux.
Todavía no se compiló ni ejecutó este instalador en Windows. El manifiesto
`windows-runtime.json` está deliberadamente sin revisar; el empaquetado se detiene
hasta completarlo con los archivos reales del SDK Windows.

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
al desinstalar y no cierra una instancia activa. La firma Authenticode, la firma
de actualizaciones y la restauración automática de Windows siguen pendientes.

Referencias: [redistribución de Visual C++](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170),
[dependencias que redistribuir](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute?view=msvc-170),
[instalación sin elevación de Inno Setup](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm).

## Curador desde el repositorio

Para colaborar con Linux sin generar un instalador, compilar y ejecutar
`run-curated.bat`. Ver [guía de curación compartida](../mds/CURADOR_COMPARTIDO.md).
