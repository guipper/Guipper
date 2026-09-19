# Transiciones de composición

El compositor se comparte entre MAIN, grupos, CUE y XML. Los identificadores
históricos siguen siendo Mix=0, Warp=1 y Bayer=2. Los nuevos perfiles usan
fundido, 1,5 segundos, inicio inmediato y calidad automática.

## Flujo

- L, OSC, recuperación y XML arrastrado como composición solicitan una carga.
- Lectura, validación y expansión del árbol XML ocurren en un trabajador CPU.
- Se construye un recurso por frame en el contexto GL, también dentro de grupos.
  Los programas de shader se reutilizan por fuente, incluyendo sus includes.
- Se comprueban shaders enlazados, FBO, media decodificada y textura de cámara/
  video. Una imagen negra es válida. El destino se renderiza antes de instalarlo.
- Hasta completar la preparación, MAIN y el nombre del archivo activo permanecen
  en la composición anterior. Un error o 10 s sin preparación conserva esa escena.
  El aviso permite reintentar. Una petición nueva reemplaza al candidato pendiente.
- Al presentar, se retiene el grafo saliente y su FINAL. Una interrupción utiliza
  la mezcla visible capturada. Clear cancela el candidato y libera escenas retenidas.
- Las salidas MAIN, recortes y exportaciones leen el mismo resultado. Las salidas
  directamente vinculadas a nodos mantienen su selección.

`JPboxgroup::load()` continúa siendo la variante sincrónica para inicialización y
herramientas; la carga interactiva usa `requestSessionLoad`/`pollSessionLoad`.

## Recursos internos y perfiles existentes

El compositor carga sus shaders internos desde la instalación (`AppPaths::bundle`),
no desde la copia de la biblioteca del usuario. Los perfiles de desarrollo ya
migrados pueden no contener archivos internos agregados después de su creación.
La carga verifica ambas etapas de shader antes de enlazar; un programa con sólo
vértices no es un compositor válido. Si falta el catálogo, intenta el fundido
interno y nunca conserva un programa incompleto como carga exitosa.

La regresión del catálogo crea un perfil antiguo sin `transition_catalog.frag`,
separa instalación y datos, y comprueba que la mezcla siga produciendo píxeles.
También puede ejecutarse sin `xvfb-run` para probar el contexto GPU del escritorio.

Con NDI compilado y su runtime disponible, el catálogo también ejercita la salida
asíncrona con fuentes de 80×48, 640×360 y el FBO de una transición. Comprueba
que la resolución anunciada se mantenga estable y que se conserven imagen y alpha.
Esto cubre el cierre por desbordamiento de buffers al cambiar el shader activo:
el addon no redimensiona sus buffers CPU cuando `SendImage(texture)` recibe
una textura mayor. Guipper adapta las fuentes a su FBO de salida NDI antes del envío.

## Catálogo y configuración

SETTINGS ofrece familias, favoritos y participación en aleatorio independientes,
duración en segundos/pulsos, inicio inmediato/próximo pulso/próximo grupo de cuatro,
controles del efecto y calidad. La prueba A/B usa dos fuentes animadas sintéticas;
no aplica cambios a MAIN. Los nuevos ajustes viven en `<transitions version="1">`.

El patrón se fija al iniciar. Aleatorio excluye morph/feedback incompatibles y
el efecto anterior si hay alternativas. El reloj usa el BPM maestro, convierte la
duración en pulsos al empezar y no retrocede ni se alarga por un frame lento.

## Transiciones de paleta

En SETTINGS → TRANSICIONES, **Eco cromático** y **Datamosh de paleta** están
en Orgánicos; **Glitch espectral** está en Trama. También aparecen en Todos y
pueden incluirse en Aleatorio. Intensidad regula la deformación y la memoria.

- Eco cromático: arrastra cuadros anteriores con un flujo guiado por bordes y
  mezcla los tonos de la composición que sale con los de la que entra.
- Datamosh de paleta: retiene y desplaza bloques según diferencias de color,
  transfiriendo la paleta del destino. Es una simulación visual; no usa un codec
  ni calcula optical flow.
- Glitch espectral: bandas desplazadas, separación RGB y ecos coloreados.

El análisis muestrea ambas imágenes en GPU y obtiene sombras, medios y luces,
ponderando por alpha. No lee píxeles a CPU. Dos FBO alternados conservan la
memoria de cada compositor sin leer y escribir la misma textura. El inicio y
el final devuelven las fuentes exactas; cada nueva transición descarta el historial.
Los IDs existentes (incluido Aleatorio=18) conservan su significado en XML.

La regresión `transition_catalog` guarda `palette-19/20/21-*.png`, verifica
memoria temporal y su reinicio, además de los extremos RGBA del catálogo.

## Morph y feedback

Morph exige igualdad de la fuente compilada y del esquema de nombres, tipos y
rangos nativos. Para escenas completas/CUE se emparejan identidades únicas.
La automatización conserva sus valores; el guardado usa el valor propio del
destino. Los booleanos del shader cambian al finalizar.

Categorías opcionales, declaradas en la fuente (sin inferirlas del nombre):

```glsl
// @transition-category motion zoom
// @transition-category color saturation
// @transition-category shape radius
```

Las ventanas son movimiento 0–60 %, color 20–80 % y forma 40–100 %. Sin categorías,
la variante escalonada usa morph simultáneo. Los floats sin categoría usan la
ventana completa; no se presupone una unidad angular o un espacio de color.

Feedback requiere un sampler `feedback` activo en el programa enlazado. Se
inicializa una vez; nunca se utiliza una entrada normal ni se siembra un nodo que
ambas ramas comparten. Sin compatibilidad se aplica fundido y se indica el motivo.

## Calidad

Automática utiliza el límite configurado, como máximo 60 FPS. Doce frames seguidos
por encima del presupuesto reducen 100 → 75 → 50 % por dimensión. Si sigue
excedido, captura la saliente y mantiene vivo el destino. Los shaders reducen sus
FBO reales; media y mapping mantienen sus recursos nativos. Al finalizar se
restauran los buffers, conservando feedback, antes de retirar la mezcla.

Las métricas actuales son tiempo de preparación, envío de render/composición en
CPU y tiempo entre frames; **no son consultas de tiempo GPU**. Los logs
`transition-prepare` y `transition` explican preparación y degradación. Una
compilación individual todavía puede bloquear el driver.

## Pruebas reproducibles

```sh
make -C tests transition_tests
./tests/transition_tests
xvfb-run -a python3 tests/run_transition_catalog.py
xvfb-run -a python3 tests/run_session_fade.py
xvfb-run -a python3 tests/run_cue_media.py
xvfb-run -a python3 tests/run_xml_drop.py
xvfb-run -a python3 tests/run_shader_alpha.py
```

Los runners copian datos y ejecutable a un directorio temporal y utilizan otro
perfil. No editan composiciones personales. El catálogo verifica extremos RGBA,
proporción no cuadrada, saliente animada, preparación incremental, error de
lectura y cancelación. Produce capturas en `dist/transition-catalog-shots`.
También comprueba morph sin contaminar el guardado, valores discretos, feedback
anidado y la interrupción A→B→A dentro de grupos. Las lecturas de píxeles esperan
a la GPU en los tests; esa sincronización no se ejecuta en el render normal.
La prueba de sesión cubre FINAL, primer frame, mezcla, interrupción, exportación,
recorte y destino vacío.

También se puede compilar el modelo puro mediante CMake/CTest en Windows/macOS.
Los runners nativos admiten `--binary bin/Guipper.exe` con escritorio Windows.

## Validación que requiere el entorno real

- Comparar visualmente `tommy/3.xml` y las parejas de composiciones del show.
- Cámara, video, varias pantallas, mapping y CUE simultáneos bajo carga real.
- Presión de memoria y compilaciones que bloqueen el driver.
- Compilar/ejecutar en Windows y comprobar OpenGL/Spout; Linux no verifica Spout.
- Revisar artísticamente plasma, radial y warp: las pruebas de píxeles no deciden
  si una transición resulta apropiada para una pareja concreta.

CUE conserva una copia de render de la escena saliente; esa copia se construye
al aplicar. Aunque reutiliza programas y captura de cámara, una escena grande
puede producir un pico de asignación. La previsualización A/B es del efecto,
no un segundo editor de composiciones.
