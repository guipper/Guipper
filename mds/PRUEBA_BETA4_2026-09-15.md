# Beta.4: UI de actualizaciones y changelog

Publicado: https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.4

Commit de versión: `517692fca91677ae1a8d125ddf6c2a37162e9a1b`.
Las notas de la release y del canal proceden de la entrada beta.4 de `CHANGELOG.md`.

## Validación

- Release compila; 13 suites C++ y 17 tests Python aprobados.
- Workflows validados con actionlint. Core tests de main aprobados en GitHub.
- UI verificada antes del empaquetado: ocho estados, tamaños 1080×780 y 400×430,
  idiomas inglés/español, cierre con mouse y Escape, preferencias mediante
  atajos, Home/End y bloqueo del cierre durante instalación. Backend simulado;
  no descarga ni instala durante estas capturas.
- AppImage firmado y firma verificada con la clave permanente de Guipper.
- Arranque saludable del paquete firmado con perfil temporal.
- Descarga pública comparada con el SHA256 antes de cambiar el canal beta.
- Actualizador original de beta.3: consulta del canal público, descarga,
  verificación de firma, instalación y arranque de beta.4 aprobados.
- `.previous` conserva beta.3; los dos archivos de prueba del perfil mantienen
  sus hashes.

SHA256 beta.4:
`72f78957723bedc841a6522943eef868f82a50371e52fdc8b82157dba964fa33`

Evidencia: `dist/beta4-validation-20260915/` y
`dist/release-panel-validation-20260915/`.

El canal beta apunta a beta.4; stable permanece sin activar. El recorrido público
usa directamente los componentes de actualización, sin pulsar botones F10.
La confirmación manual final en el equipo del usuario queda pendiente.

El workflow de release ya es válido y la ejecución de la etiqueta beta.4 quedó
en cola esperando runners propios. El paquete publicado es el generado y
validado localmente. Windows y sesiones de dos horas siguen fuera de esta prueba.
