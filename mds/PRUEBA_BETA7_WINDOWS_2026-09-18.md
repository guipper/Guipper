# Beta.7 Windows — Random al final

Cambio de presentación en SETTINGS: recorrer todos los efectos por su ID salvo
Random, que se presenta último. Los filtros se aplican sobre ese orden, incluido
Favoritos. Los IDs persistidos no se cambian (Random sigue siendo 18).

Release x64 compilado con el mismo runtime y clave pública que beta.6.
Instalador: `Guipper-0.1.0-beta.7-windows-x64.exe`.
SHA256: `7a17e68058f44f5ae16e47a768f6cec2e5533ac11bafc49dce7ab75f16d1360f`.

## Pruebas

- Probe con versión beta.6, backend de producción y DLL real de WinSparkle.
- Descarga beta.7 firmada: aceptada y staged con SHA256 idéntico.
- Bytes alterados, firma ausente o incorrecta: rechazados sin staging.
- Sin conexión: error; cancelación: no deja instalador listo.
- Instalación del payload verificado con perfil aislado, usando el bundle beta.6
  publicado como A y beta.7 como B. Inicio, shaders, composición y preferencias
  personales comprobados mediante health check y SHA256; A sigue disponible.

Resultados locales en `dist/beta7-update-validation` y
`dist/beta7-install-validation`. No se modifica la instalación habitual del usuario.
El servidor local puede registrar conexión cerrada durante la cancelación esperada.

La validación automatizada usa el backend real, no clics en F10. La prueba manual
consiste en abrir beta.6, F10, canal Beta, buscar, descargar e instalar beta.7;
comprobar que Random es el último botón de la lista de SETTINGS.
Se mantienen las limitaciones beta.6: sin Authenticode, sin prueba de máquina
limpia o sesión prolongada, NDI 6 externo y sin rollback automático Windows.
