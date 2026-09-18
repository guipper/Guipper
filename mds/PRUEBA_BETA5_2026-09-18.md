# Beta.5: transiciones, tipografía, IMPORT y edición de texto

Publicado: https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.5

Commit de versión: `9fe901507278f8c2d0fa2ee1045093ba236244f2`.
Las notas resumen la entrada beta.5 de `CHANGELOG.md`.

## Validación

- Release Linux compilada; dependencias del SDK verificadas.
- 18 suites C++ y 21 tests Python aprobados; Core tests de main y etiqueta aprobados en GitHub.
- Regresiones GPU `transition_catalog` y `session_fade` aprobadas en Xvfb.
  Incluyen recursos ausentes, compatibilidad de morph/feedback y la corrección del negro durante transiciones.
- Recursos Overpass y `transition_catalog.frag` incluidos en la allowlist con hashes; licencia OFL incluida.
- AppImage firmado con la clave permanente y firma verificada.
- Arranque saludable del AppImage con perfil aislado; captura revisada para comprobar texto y escena inicial.
- Pruebas nativas de actualización: firma válida, clave incorrecta, paquete sin firma,
  alteración, crash/rollback, cancelación, sin conexión, ejecutable renombrado y conservación de datos aprobadas.
- Descarga pública comparada por SHA256 antes de modificar el canal beta.
- Actualizador e instalador originales de beta.4: consulta pública, descarga, firma,
  instalación y arranque saludable de beta.5 aprobados.
- `.previous` conserva beta.4 y los archivos de prueba del perfil mantienen sus hashes.

SHA256 beta.5:
`d01833f63a79a9d804820d3336911f1f679fe0faf3743afef61fcad651e5f47a`

Huella de firma:
`EAE485030F022D467FCFD36E0F659D1854076536`

Evidencia local: `dist/beta5-validation-20260918/`,
`dist/beta5-transition-catalog.log` y `dist/beta5-session-fade.log`.

## Alcance y reproducción

El canal beta apunta a beta.5. Se conservó el canal stable.
La prueba pública invoca los componentes originales del actualizador sin pulsar F10;
no cambia la instalación ni el perfil habitual del usuario.

El paquete se compiló y firmó localmente. El workflow de empaquetado de la etiqueta
quedó en cola a la espera de runners propios; los tests alojados por GitHub sí pasaron.
El SDK de actualización se reconstruyó con las revisiones fijadas por el repositorio.
Los headers de desarrollo ausentes se extrajeron de paquetes Ubuntu en un directorio
local de build, sin modificar la instalación del sistema.

Windows/macOS, Spout, hardware de cámara y sesiones prolongadas no fueron validados
para esta publicación. No se incluye la biblioteca personal ni las colecciones
contribuidas pendientes de revisión. La investigación RGB depth permanece pausada.
