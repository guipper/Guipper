# Beta.2: corrección de Alt y prueba de actualización

## Cambio

Alt abría el debug avanzado porque `OF_KEY_ALT` y el carácter de control
Ctrl+D comparten el valor 4. El atajo ahora requiere la tecla física D,
Ctrl/Cmd y ausencia de Alt. Versión preparada: `0.1.0-beta.2`.

## Validación local del teclado

Ejecutada con Xvfb y Mesa llvmpipe, perfiles temporales independientes, sin
importar el perfil personal. Beta.1 se abrió desde su AppImage firmado;
beta.2 desde el AppDir preparado para empaquetar.

| Caso | Resultado |
|---|---|
| Beta.1: Alt izquierdo abre debug | Fallo reproducido |
| Beta.2: Alt izquierdo no abre debug | Aprobado |
| Beta.2: Alt derecho no abre debug | Aprobado |
| Beta.2: Ctrl+Alt no abre debug | Aprobado |
| Beta.2: Ctrl+D abre debug avanzado | Aprobado |
| Beta.2: Alt no cierra el debug abierto | Aprobado |
| Beta.2: Ctrl+D cierra debug avanzado | Aprobado; mantiene el readout simple existente |
| Compilación Release | Aprobada |
| `make -C tests run` | 13 suites aprobadas |
| Tests Python `release_*_tests.py` | 17 tests aprobados |

Capturas y logs locales: `dist/beta2-validation-20260915/` (no versionados).

## Actualización A → B

Aprobada localmente con los AppImages completos beta.1 y beta.2, firmados con
la clave `EAE485030F022D467FCFD36E0F659D1854076536`. Se usaron el worker y el
instalador incluidos en beta.1, un servidor loopback y un perfil temporal.

| Comprobación | Resultado |
|---|---|
| Descarga y verificación de firma de beta.2 | Aprobado |
| Beta.1 permanece intacta antes de instalar | Aprobado |
| Paquete instalado coincide con el hash de beta.2 | Aprobado |
| Arranque saludable y F10 muestra beta.2 | Aprobado |
| `.previous` coincide con el hash de beta.1 | Aprobado |
| 35 archivos XML y shaders del perfil conservan sus hashes | Aprobado |
| Alt no abre debug después de actualizar | Aprobado |

El primer intento de firma agotó el tiempo de espera; el reintento se firmó y
verificó correctamente. El primer recorrido de instalación falló porque el
script de prueba esperaba la marca de salud fuera de la caché del perfil.
Se corrigió esa ruta en el script y el recorrido completo pasó; no fue
necesario modificar el código de instalación.

SHA256 de beta.1:
`e3d9ad09dfe82013a5132ff517f6a219514d6e023b7359585bda47ceda6e0c7f`

SHA256 de beta.2:
`ecc259905b663c3edd8b6b3e6a9d5044ede382631d6b1d92b7f71e90a5d45384`

Artefacto: `dist/signed-beta2-20260915/Guipper-0.1.0-beta.2-linux-x64.AppImage`.
Feed preparado: `dist/signed-beta2-20260915/channel-beta/Guipper-linux-x64.AppImage.zsync`.
Evidencia: `dist/beta2-validation-20260915/update/`.

El feed loopback se pasó directamente al worker de beta.1. Esta prueba no
valida la consulta pública ni las acciones de descarga e instalación desde
F10. No se publicó ninguna release ni se modificaron canales. Tampoco valida
Windows, equipos limpios ni estabilidad durante una actuación.
