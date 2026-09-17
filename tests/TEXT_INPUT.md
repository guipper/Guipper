# Edición de texto común

`jp_text_edit.h` contiene el modelo UTF-8, selección y deshacer/rehacer sin
openFrameworks ni OpenGL. `jp_text_input.h` adapta eventos y portapapeles,
coordina el foco y dibuja los campos con las fuentes de Guipper. Los paneles
registran los campos visibles y aportan validación y acciones de confirmar o
cancelar. Los cambios no alteran formatos guardados.

## Uso

- Flechas, Shift+flechas, Home/End y Ctrl+Home/End mueven o seleccionan texto.
- Ctrl+flechas y Ctrl+Backspace/Supr operan por palabras.
- Ctrl+A/C/X/V y Ctrl+Z/Shift+Z/Y actúan solo sobre el campo.
- En macOS: Cmd para portapapeles y deshacer, Option para palabras y Cmd+flechas
  para extremos. Ctrl+Alt no se interpreta como atajo de portapapeles (AltGr).
- Clic, arrastre, doble clic y Shift+clic editan la selección.
- Enter confirma; Escape cancela la edición y libera el foco. Tab y clic fuera
  confirman valores válidos. Un valor inválido conserva texto, selección y foco,
  con una explicación junto al campo.
- Búsqueda de IMPORT: actualiza al escribir, flechas verticales recorren resultados,
  Enter carga y Escape conserva la búsqueda.
- IN/OUT: flechas verticales ajustan el borrador por frames; Enter lo aplica.
- Comentarios: Enter agrega una línea, Ctrl/Cmd+Enter publica; salir del campo
  conserva el borrador sin publicarlo.

El editor de shaders y los diálogos del sistema conservan su edición propia.

## Pruebas sin interfaz

Linux:

```sh
make -C tests run
```

Visual Studio compila las fuentes con `/utf-8` para conservar acentos y ñ.

El proyecto CMake permite ejecutar las mismas pruebas del modelo en los tres
sistemas. Se puede elegir el generador `Visual Studio 17 2022` o `Xcode`:

```sh
cmake -S tests -B tests/build-text
cmake --build tests/build-text --config Release
ctest --test-dir tests/build-text -C Release --output-on-failure
```

## Pruebas con la aplicación

Después de compilar Guipper:

```sh
xvfb-run -a -s '-screen 0 1920x1080x24' python3 tests/run_text_input.py
```

En Windows, desde un escritorio:

```bat
python tests/run_text_input.py --binary bin/Guipper.exe
```

El runner copia ejecutable y datos a un directorio temporal y usa un perfil
separado. No trabaja sobre las composiciones del usuario. El registro queda en
`dist/text-input.log` y las capturas ES/EN amplias/compactas en
`dist/text-input-captures/`.

Las pruebas ejercitan selección, edición UTF-8, borrado por palabras, límites,
deshacer agrupado, pegado, foco, rechazo de clics, Tab, Escape, búsqueda y el
modal de guardado, incluido un fallo real de escritura que conserva el borrador.
Las conversiones IN/OUT se prueban con tiempos, frames y entradas inválidas. Los eventos reales de OF comprueban que pegar no se duplica
y que editar no dispara acciones de guardar ni borrar nodos.
IMPORT se prueba entrando por su ruta real, con el catálogo completo: escritura,
búsqueda sin resultados, borrado del filtro y cambios de nombres y marcas.
El log mide tres dibujos consecutivos con búsqueda activa para detectar bloqueos
por recalcular el catálogo en cada consulta de geometría o interacción.


## Verificación pendiente en Windows/macOS

La compilación y ejecución nativa de Guipper en esos sistemas deben comprobarse
allí. La ejecución Linux no las sustituye. Revisar además:

- Portapapeles entre aplicaciones, AltGr, acentos y ñ con teclados locales.
- Cmd/Option, selección con Shift y repetición de teclas en macOS.
- Escalas HiDPI y arrastre de selección fuera del campo.
- Nombres, rutas, hexadecimales, MIDI, IN/OUT y comentarios multilínea con
  controles y modales detrás; ningún gesto debe propagarse.

No se incorporó un sistema nuevo de composición IME, menú contextual ni
completado automático.
