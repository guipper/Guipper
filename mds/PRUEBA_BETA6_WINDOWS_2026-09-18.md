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
