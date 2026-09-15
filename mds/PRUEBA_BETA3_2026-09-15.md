# Beta.3: aleatorización manual y actualización pública

Publicado: https://github.com/guipper/Guipper/releases/tag/v0.1.0-beta.3

Commit de versión: `79921422f87182665a5ff3307f0cfda8fd772da3`.

Se eliminó la aleatorización por doble clic derecho que también se activaba
al combinar clic izquierdo y derecho. La acción manual se conserva únicamente
con clic izquierdo en el botón superior RDM, respetando límites y bloqueos.

## Validación

- Compilación Release aprobada.
- Las 13 suites C++ pasaron con el fix; los 17 tests Python de release pasaron
  con la versión beta.3.
- AppImage firmado con `EAE485030F022D467FCFD36E0F659D1854076536` y firma verificada.
- Arranque del AppImage aprobado en Xvfb/Mesa con perfil temporal y marca de salud.
- Descarga pública verificada antes de actualizar el canal beta.
- Actualizador e instalador originales de beta.2: consulta del canal público,
  descarga, firma, reemplazo y arranque de beta.3 aprobados.
- `.previous` coincide con beta.2 y los dos archivos de prueba del perfil
  conservan sus hashes.

SHA256 beta.3:
`c6dd258f2b2938db6e71a2852394d735ed9171d7f2f1c5548dc65b03fba64825`

Evidencia local: `dist/beta3-validation-20260915/`.

El canal beta apunta a beta.3; stable permanece sin activar. La comprobación
usó directamente los componentes de actualización, sin pulsar los botones de
F10. Quedan pendientes la confirmación manual del gesto del mouse y de F10
en el equipo del usuario. No se validaron Windows ni sesiones de dos horas.
