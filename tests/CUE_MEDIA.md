# Regresión de medios en CUE

Después de compilar Guipper en Linux:

```sh
xvfb-run -a python3 tests/run_cue_media.py
```

En Windows, con un escritorio disponible:

```bat
python tests/run_cue_media.py --binary bin/Guipper.exe
```

El runner copia los datos y el ejecutable a un directorio temporal y usa un
perfil aislado. No modifica las composiciones del usuario. El log queda en
`dist/cue-media.log`.

La prueba genera una imagen roja y compara los píxeles de CUE con la salida
live al cambiar el zoom, cancelar y aplicar. Repite la edición dentro de un
grupo. Para CAMARITA comprueba que el borrador tiene su propio renderer y
parámetros, y que comparte la captura del dispositivo con la caja live cuando
hay una cámara disponible. Puede activar brevemente esa cámara durante la
prueba; no guarda imágenes de ella.

La comprobación visual con una cámara real consiste en activar CUE, cambiar
zoom y desplazamiento y verificar que la ventana CUE cambia mientras la salida
live conserva sus valores hasta aplicar. Repetir dentro de un grupo.
Las cajas de vídeo, NDI y Spout conservan su comportamiento anterior.
