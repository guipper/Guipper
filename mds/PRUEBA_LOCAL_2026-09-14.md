# Prueba local Linux — 14 de septiembre de 2026

**Resultado: recorrido NO aprobado.** Las pruebas automáticas pasan y la recuperación funciona, pero se detectaron cuatro fallos funcionales en el paquete probado.

## Entorno y artefacto

- Ubuntu 24.04.5 LTS x64; pantalla Xvfb dedicada, Mesa 25.2.8 / llvmpipe LLVM 20.1.2, OpenGL 4.5. Renderizado por software: no mide rendimiento de GPU física.
- Versión `0.1.0-beta.1`; AppImage reconstruido el 14/09 a las 13:11 (Argentina), iniciado con `APPIMAGE_EXTRACT_AND_RUN=1`.
- SHA256: `04b6f11fa99b7aac222735ceb20848b1f20095794b575489411505ae66d4235f`.
- Perfil exclusivo: `/tmp/guipper-local-20260914-f_584u60/profile`, mediante `GUIPPER_USER_ROOT`; sin `GUIPPER_LEGACY_DATA`. Proyectos, shaders editados y recuperación quedaron dentro de ese perfil.
- Interacción automatizada con teclado/ratón sobre la aplicación real; no es una sesión realizada por un tester humano. La extracción evita depender de FUSE y no valida ese mecanismo de montaje.
- Se observaron cambios concurrentes en `scripts/release/demos.py` posteriores al empaquetado. Estos resultados describen exclusivamente el hash anterior, no certifican esos cambios posteriores.
- [Evidencias locales](../dist/local-test-20260914-f_584u60/): capturas, logs, diagnóstico y valores comparados. Esa carpeta está excluida de Git.

## Resultados

| Prueba | Resultado | Evidencia |
|---|---|---|
| 13 suites C++ | Aprobado | `core.log` |
| 3 pruebas Python de packaging/helper | Aprobado | `python.log` |
| Compilación Release y AppImage | Aprobado | `build.log`, `package.log` |
| Regresiones OpenGL en copia de bin/data | Aprobado | `opengl.log`, incluidos load-safety y persistence-modules |
| Primer inicio, densidad y color | Aprobado | `01-startup.png`, `03-parameters-changed.png` |
| Animación autónoma de anillos | **Fallido** | `13-animation-a.png`, `14-animation-b.png` |
| Entrada de audio / analizador | Aprobado con señal sintética | `24-audio-silence.png`, `27-audio-tone.png` |
| Reacción visual del ejemplo de audio | **Fallido** | `25-visual-silence.png`, `26-visual-tone.png` |
| Composición de mezcla | **Fallido** | `28-mix-controls.png`, `29-mix-zero.png`, `30-mix-one.png` |
| Guardado confirmado y reapertura | Aprobado | `21-reopened-saved.png`, `33-mix-reopened.png`; parámetros y dos conexiones conservados |
| Cancelar Guardar como sin modificar original | **Fallido** | `save-as-before.xml.txt`, `save-as-after.xml.txt`, `32-save-as-before-confirm.png` |
| Shader personal y reinicio | Aprobado | Comentario añadido con editor y conservado; copia del paquete sin modificar |
| F10, tamaño normal/pequeño, scroll y Escape | Aprobado | `07-f10-normal.png` a `10-escape-closed.png` |
| Preferencias de actualizaciones / diagnóstico | Aprobado | `15-update-preferences.png`, `diagnostics.json`; se restauraron stable y consulta diaria desactivada |
| Recuperación tras cierre forzado | Aprobado | `16-recovery-offered.png`, `17-recovered.png`, `verification.json` |

La recuperación esperó **130,012 segundos** después del cambio. Se obtuvo el PID de la ventana, se comprobó su perfil en el entorno del proceso y se envió SIGKILL solo a esa instancia. Al reiniciar, F9 recuperó `density=0.272455`; el archivo guardado previamente conservaba `0.811377`. Se guardó la recuperación como `savefiles/recovered-verified.xml` y se verificaron sus bytes XML.

El audio se probó con silencio y una senoide de 80 Hz a 48 kHz en un sink virtual exclusivo. Solo el flujo de captura de la instancia de prueba fue redirigido. El medidor Low pasó de 0,00 a aproximadamente 0,99, pero la imagen no cambió. Se cerraron las instancias de prueba y se retiró el sink virtual al terminar.

## Fallos reproducibles y siguiente corrección

### 1. Guardar como escribe antes de confirmar — prioridad alta

1. Abrir una composición guardada y cambiar `amount`.
2. Pulsar Ctrl+Shift+S sin confirmar el diálogo.
3. Comparar el XML original: ya contiene el nuevo valor. Escape no revierte esa escritura.

Esperado: Guardar como no modifica el origen y Cancelar no escribe. Observado: el atajo ejecuta primero el guardado de `s` y luego abre el diálogo. Los logs y las copias antes/después confirman el cambio. Revisar el despacho duplicado entre `keyPressed` y `keycodePressed`; agregar una regresión del atajo completo, no solo de la función de guardado.

### 2. Anillos estáticos

Esperado: animación continua sin tocar controles. Observado: dos capturas separadas por dos segundos tienen píxeles idénticos en el área de la visual. `time` aparece como parámetro editable constante. La inspección del código muestra que los parámetros no globales se aplican después de los globales; este shader no marca `time` como interno. Corregir el contrato del ejemplo y verificar de nuevo su salida durante varios fotogramas.

### 3. Audio medido pero sin reacción visual

Esperado: el tono grave aumenta el brillo. Observado: silencio y tono generan la misma imagen, pese al cambio del analizador. El ejemplo usa `audio_low`, mientras la publicación de globals inspeccionada proporciona `audio_bands`. Alinear el shader de ejemplo con el contrato real y repetir silencio → tono → silencio.

### 4. Mezcla negra

Esperado: `amount` interpola entre anillos e imagen. Observado: salida y preview negros con valores aproximadamente 0,01, 0,50 y 0,99; el panel indica 2/2 entradas conectadas. Revisar compatibilidad de sampler, target y coordenadas de textura del shader generado. La causa exacta aún no está confirmada; no basta que compile o que conserve los enlaces.

También se observó superposición entre el título del inspector de nodos y las pestañas superiores a 1080×750 (`28-mix-controls.png`). No impidió la prueba, pero debe corregirse antes de promocionar capturas del producto.

## Límites y repetición

No se validaron Windows, actualización A → B, firmas reales, FUSE, instalación en máquina sin SDK, NDI, MIDI, salida externa, GPU física ni una actuación de dos horas. El runtime NDI no estaba disponible. Las actualizaciones desactivadas son el comportamiento esperado de este candidato.

Para repetir las suites:

```sh
make -C tests run
python3 -m unittest discover -s tests -p 'release_*_tests.py'
```

Las regresiones OpenGL deben ejecutarse con `GUIPPER_PERSISTENCE_TEST=1` desde una copia temporal del ejecutable y `data`; ese modo escribe fixtures. Para el recorrido gráfico, usar un perfil nuevo y el AppImage del hash registrado, confirmar los diálogos antes de pasar al siguiente paso y no confundir carga sin errores con salida visual correcta.

No se cambiaron fuentes de la aplicación durante esta ronda ni se hicieron commits. Antes de publicar, corregir los cuatro fallos y repetir el recorrido sobre un nuevo artefacto identificado por hash.

## Verificación posterior — cuatro correcciones

Los cuatro fallos funcionales anteriores están **corregidos y verificados localmente**.
El registro inicial se conserva como evidencia de reproducción; esta sección corresponde
al nuevo AppImage, SHA256 `2400f7c491f8b0bc0838ff89f6d5c9a79d3ab2f73e24dd4169512fc7b3a7982e`.

- Ctrl+S y Ctrl+Shift+S se despachan una sola vez. Se comprobó mediante eventos
  de openFrameworks y con teclado sobre la app que abrir y cancelar ambos diálogos
  conserva exactamente los bytes del original. La tecla `s` sola sigue guardando.
- El ejemplo declara `time` como interno: los píxeles cambian entre fotogramas.
  Se conservaron los controles de densidad, velocidad, nitidez y cantidad de audio.
- El ejemplo usa `audio_bands.x`, que sí publica Guipper. Con velocidad cero,
  el promedio rojo de una región pasó de aproximadamente 62 en silencio a 103
  con el tono de 80 Hz y volvió a 62 al detenerlo.
- La mezcla usa `sampler2D` y coordenadas normalizadas, compatibles con las
  texturas de Guipper. Se verificaron anillos, imagen y mezcla intermedia.
- Pasaron las 13 suites C++, las tres pruebas Python y las regresiones OpenGL
  completas, incluida la nueva regresión `save-shortcut`. El AppImage final
  también arrancó y pasó la cancelación del diálogo nativo.
- El empaquetador ahora genera un archivo temporal y solo reemplaza el candidato
  al terminar: una app abierta o un fallo de empaquetado no deben truncar el anterior.

[Evidencias de las correcciones](../dist/four-fixes-20260914/).
Se usó un perfil separado, sin reemplazar shaders personales. En perfiles existentes,
las copias oficiales sin editar se refrescan; un shader marcado personal se conserva.
Los límites de plataforma/hardware del informe inicial siguen vigentes. La superposición
del inspector con las pestañas queda como mejora visual pendiente y no forma parte de
estas cuatro correcciones. No se hicieron commits.
