# Importación de composiciones arrastradas

```sh
xvfb-run -a python3 tests/run_xml_drop.py
```

En Windows con escritorio: `python tests/run_xml_drop.py --binary bin/Guipper.exe`.

La prueba usa un perfil y archivos temporales. Simula un XML dentro de
`Guipper/bin/data/savefiles/`, con espacios en el nombre y una carpeta padre
que contiene `data`. Crea además una copia inválida con el mismo nombre en el
perfil: ambas modalidades deben abrir el archivo seleccionado.

Comprueba que el grupo contiene sus nodos, queda activado, admite pausa/reanudar,
y que un archivo faltante no agrega una caja vacía. Cubre drops vacíos y
reconocimiento de la extensión `.XML`.

Los drops conservan la ruta absoluta seleccionada. Las rutas internas antiguas
se normalizan por separado, respecto de la raíz de recursos del perfil.

## Composiciones antiguas de Windows

`xvfb-run -a python3 tests/run_legacy_composition.py` carga el archivo real
`savefiles/tommy/3.xml` con un directorio de recursos diferente de `bin/data`.
Verifica los nueve nodos tanto en modo grupo como composición y pausa/reanudar.
Las rutas internas `data\shaders\...`, `data/shaders/...` y `./data/shaders/...`
se resuelven respecto de la raíz actual, sin agregar un segundo `data`.

Si la validación falla, el aviso empieza con el nombre del recurso involucrado.
