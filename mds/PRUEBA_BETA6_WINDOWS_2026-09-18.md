# Beta.6 Windows x64 — 2026-09-18

Instalador: `Guipper-0.1.0-beta.6-windows-x64.exe`.
SHA256: `d6c60287359b6e493389f47a603690c3d2afb5e07fdf0026fbfeb9f4c4f74e69`.

Compilado Release x64 con Visual Studio 18 y openFrameworks 0.12.1.
WinSparkle 0.9.4; Inno Setup 6.7.3. DLLs MSVC app-local provenientes del
directorio redistribuible oficial de Visual Studio, con licencias y hashes en
`release/windows-runtime.json`. Imports normales y diferidos comprobados.
Instalador con firma Ed25519 separada; sin certificado Authenticode.

## Resultados

- Lock de dependencias Windows verificado.
- Pruebas runtime: 9 pasan, 1 omitida por falta de privilegio de symlinks Windows.
- Prueba de empaquetado: pasa.
- Catálogo de transiciones en GPU con el ejecutable candidato: pasa, incluyendo
  historial de paleta y cambio de tamaño de la salida NDI.
- Backend de producción y DLL WinSparkle reales: descarga válida aceptada;
  bytes alterados, firma ausente y firma incorrecta rechazados sin staging.
  Sin conexión devuelve error; cancelación no deja instalador listo.
- Instalación silenciosa sin administrador del mismo payload verificado: pasa.
- Inicio de la versión instalada: health check y proceso saludable.
- Shader personal, composición XML y preferencias JSON conservan sus SHA256.
- La versión anterior vuelve a ejecutarse usando el mismo perfil aislado.

Evidencia local: `dist/beta6-update-validation/results.json`,
`dist/beta6-install-validation/results.json`, logs vecinos y
`dist/beta6-transition-catalog.log`. Los perfiles y binarios habituales del usuario
no se sustituyeron. Las pruebas usan `GUIPPER_USER_ROOT` aislado.

## Alcance

Es el primer release público Windows. El probe simula versión beta.5 para probar
la oferta de actualización; no representa una versión Windows publicada anterior.
La prueba de instalación usa el ejecutable de desarrollo anterior como A y el
instalador beta.6 como B. Prueba el backend real, no clics de aceptación en F10.
La primera instalación se descarga manualmente; las siguientes podrán usar F10.

No se probó todavía una máquina limpia sin Visual Studio, una sesión de dos horas
ni hardware externo audio/MIDI/NDI. NDI 6 requiere runtime externo. No se anuncia
certificación de esos escenarios. Sin Authenticode, Windows puede mostrar SmartScreen.
No hay rollback automático Windows; sí se comprobó volver a ejecutar A.
La publicación es beta, no estable; el canal Linux permanece en beta.5.

## Verificación posterior a publicar

Publicado [v0.1.0-beta.6](https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.6)
desde `48278db5414bc6605c64fb196525b509b40755d0`, con instalador, SHA256 y firma.
La descarga anónima de GitHub coincide con el SHA256 anterior y su firma es válida.
El probe WinSparkle consultó `windows-beta/appcast.xml`, detectó beta.6, descargó
y verificó el instalador público; el archivo staged conserva el mismo SHA256.
Una primera consulta inmediatamente después de publicar dio error; la repetición
completó Checking → Available → Downloading → Ready. No se identificó la causa del
error inicial. El canal `windows-stable` tiene un appcast vacío.

También pasan CTest (text_edit y transition) y session-fade con el candidato.
Se desinstaló la instalación temporal B con su propio desinstalador (exit 0);
los registros y el perfil aislado permanecen disponibles. Los binarios publicados
son los compilados y probados localmente; no se atribuye esa validación al workflow
de empaquetado en runners dedicados.
