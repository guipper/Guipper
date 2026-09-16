# Clasificación de shaders — borrador de trabajo

Fecha: 2026-09-15. Fuente: archivos locales, incluidos cambios sin commit.

Documento para organizar la biblioteca antes de trasladarla a IMPORT. No modifica el programa, los shaders, las rutas ni los catálogos JSON. La estructura de categorías y etiquetas está acordada; las fichas individuales quedan pendientes de revisión con Nico. No hay cuota de selección.

## Criterios de clasificación

| Categoría principal | Función |
|---|---|
| Generador | Produce una imagen procedural sin necesitar una imagen externa como base. |
| Efecto | Transforma una imagen; puede usar otras entradas como máscara o mapa auxiliar. |
| Mezclador | Combina varias imágenes como fuentes de la composición. |
| Por definir | Requiere revisar su función antes de decidir. |

Familias visuales propuestas como etiquetas combinables: color, geometría, patrones, ruido, fractales, partículas, distorsión, simetría, glitch, pixelado, contornos, composición, máscaras, feedback.

Audio, feedback y cantidad de entradas se registran como capacidades o requisitos. No determinan por sí solos la categoría: dos texturas también pueden ser una imagen y su mapa de distorsión. La carpeta actual es una pista, no una decisión.

Estados: **Propuesta → Revisado en código → Revisado visualmente → Acordado**.
Acordar la clasificación no implica seleccionar para distribución. No se eliminan variantes ni duplicados durante esta organización.

## Método de revisión

1. Revisar tandas de 6–10 shaders de una familia en el chat.
2. Acordar nombre visible, categoría, etiquetas y descripción breve.
3. Ver la salida cuando el código no sea suficiente.
4. Registrar los acuerdos aquí.
5. Preparar la integración en IMPORT cuando la organización esté definida.

## Primeras 36 fichas propuestas

Base recuperada de la preselección local, sin limitar el universo a clasificar. Las ocho fichas de la tanda de color y patrones fueron revisadas en código el 2026-09-16; las demás siguen como propuestas. Los nombres y la clasificación de la tanda 1 fueron acordados con Nico; su validación visual sigue pendiente.

| Shader | Nombre visible propuesto | Categoría | Etiquetas | Qué hace (borrador) | Estado |
|---|---|---|---|---|---|
| [generative/basic2.frag](../bin/data/shaders/generative/basic2.frag) | Barrido senoidal | Generador | color, patrones | Bandas verticales animadas: oscila el canal rojo; verde y azul quedan fijos en 0.5 y 1. Sin controles propios. | Clasificación y nombre acordados; visual pendiente |
| [generative/degrade.frag](../bin/data/shaders/generative/degrade.frag) | Patrones de dos colores | Generador | color, patrones | Interpola dos colores con patrones lineales, radiales o angulares; permite variar forma de onda, frecuencia y velocidad. | Clasificación y nombre acordados; visual pendiente |
| [generative/solidcolor.frag](../bin/data/shaders/generative/solidcolor.frag) | Color sólido | Generador | color | Genera un color RGB uniforme. `mivariable` no interviene en la salida. | Clasificación y nombre acordados; visual pendiente |
| [generative/simplelines.frag](../bin/data/shaders/generative/simplelines.frag) | Franjas | Generador | geometría, patrones | Franjas monocromas horizontales o verticales, con frecuencia, velocidad y umbral de intensidad ajustables. | Clasificación y nombre acordados; visual pendiente |
| [generative/simplelines_2.0.frag](../bin/data/shaders/generative/simplelines_2.0.frag) | Gradiente rojo-verde (experimental) | Generador | color | La salida actual es vec4(uv.x, param1, 0, 1). Los cálculos de franjas no llegan a la salida; no es una segunda versión funcional de Franjas. | Clasificación y nombre acordados; visual pendiente |
| [generative/grid.frag](../bin/data/shaders/generative/grid.frag) | Grilla de polígonos | Generador | geometría, patrones | Repite polígonos con cantidad de lados, escala, desplazamiento y rotación ajustables; suma feedback según feedbackst. | Clasificación y nombre acordados; visual pendiente |
| [generative/quad.frag](../bin/data/shaders/generative/quad.frag) | Cuadrado pulsante | Generador | geometría | Polígono de cuatro lados con desplazamiento, escala por eje y pulsación de tamaño. Lee feedback pero no lo incorpora a la salida. | Clasificación y nombre acordados; visual pendiente |
| [generative/anillos3D.frag](../bin/data/shaders/generative/anillos3D.frag) | Anillos 3D | Generador | geometría, 3D, repetición | Anillos repetidos en un espacio 3D animado, con orientación, tamaño, separación y grosor ajustables. | Revisado en código; en curado para probar |
| [generative/circuloresolution.frag](../bin/data/shaders/generative/circuloresolution.frag) | Círculo suave | Generador | geometría | Disco monocromo con posición, radio y suavidad del borde ajustables. | Revisado en código; en curado para probar |
| [generative/circuloresolution2.frag](../bin/data/shaders/generative/circuloresolution2.frag) | Gradiente XY (experimental) | Generador | color | Gradiente rojo horizontal y verde vertical, con azul fijo. Sus controles actuales no afectan la salida. | Revisado en código; en curado para probar |
| [generative/allpaterns.frag](../bin/data/shaders/generative/allpaterns.frag) | Mezcla de ruidos | Generador | ruido, patrones | Interpola siete campos procedurales, entre ellos ruido, Voronoi y fBm; permite escala y desplazamiento animado por eje. | Clasificación y nombre acordados; visual pendiente |
| [generative/fbm.frag](../bin/data/shaders/generative/fbm.frag) | Ruido fractal | Generador | ruido | Ruido procedural en capas con animación. | Propuesta |
| [generative/fbmvuse.frag](../bin/data/shaders/generative/fbmvuse.frag) | Ruido fractal II | Generador | ruido | Tratamiento alternativo de ruido en capas. | Propuesta |
| [generative/perlinnoisefires.frag](../bin/data/shaders/generative/perlinnoisefires.frag) | Fuego de ruido | Generador | ruido | Ruido procedural con apariencia de fuego. | Propuesta |
| [generative/ksetraps.frag](../bin/data/shaders/generative/ksetraps.frag) | Órbitas fractales | Generador | fractales | Formas fractales iterativas con color y contraste. | Propuesta |
| [generative/tunel.frag](../bin/data/shaders/generative/tunel.frag) | Túnel | Generador | geometría, 3D, repetición | Recorrido animado por un túnel de formas repetidas, con velocidad, radio y dos colores de iluminación. | Revisado en código; en curado para probar |
| [generative/FractLines.frag](../bin/data/shaders/generative/FractLines.frag) | Líneas fractales | Generador | fractales | Fractal procedural construido con líneas. | Propuesta |
| [generative/atomdistort.frag](../bin/data/shaders/generative/atomdistort.frag) | Distorsión atómica | Generador | abstracto | Estudio generativo de distorsión. | Propuesta |
| [imageprocessing/brightcontrast.frag](../bin/data/shaders/imageprocessing/brightcontrast.frag) | Brillo / contraste | Efecto | color | Ajusta brillo y contraste de una fuente. | Propuesta |
| [imageprocessing/huerotate.frag](../bin/data/shaders/imageprocessing/huerotate.frag) | Rotación de tono | Efecto | color | Desplaza los tonos de una fuente. | Propuesta |
| [imageprocessing/invert.frag](../bin/data/shaders/imageprocessing/invert.frag) | Invertir | Efecto | color | Invierte los colores de la fuente. | Propuesta |
| [imageprocessing/blackandwhite.frag](../bin/data/shaders/imageprocessing/blackandwhite.frag) | Blanco y negro | Efecto | color | Convierte una fuente a monocromo. | Propuesta |
| [imageprocessing/flip.frag](../bin/data/shaders/imageprocessing/flip.frag) | Voltear | Efecto | geometría | Invierte la orientación de la imagen. | Propuesta |
| [imageprocessing/rotate.frag](../bin/data/shaders/imageprocessing/rotate.frag) | Rotar | Efecto | geometría | Rota una imagen de entrada. | Propuesta |
| [imageprocessing/scale.frag](../bin/data/shaders/imageprocessing/scale.frag) | Escalar | Efecto | geometría | Cambia la escala de la fuente. | Propuesta |
| [imageprocessing/vignette.frag](../bin/data/shaders/imageprocessing/vignette.frag) | Viñeta | Efecto | color | Modifica el brillo en los bordes. | Propuesta |
| [imageprocessing/kaleidoscope.frag](../bin/data/shaders/imageprocessing/kaleidoscope.frag) | Caleidoscopio | Efecto | distorsión | Repite y refleja una imagen. | Propuesta |
| [imageprocessing/emboss.frag](../bin/data/shaders/imageprocessing/emboss.frag) | Relieve | Efecto | contornos | Tratamiento de imagen con apariencia de relieve. | Propuesta |
| [imageprocessing/recolor.frag](../bin/data/shaders/imageprocessing/recolor.frag) | Remapeo a dos colores | Efecto | color | Remapea una fuente entre dos colores RGB. | Propuesta |
| [imageprocessing/saturationbrightness.frag](../bin/data/shaders/imageprocessing/saturationbrightness.frag) | Saturación / brillo | Efecto | color | Ajusta saturación y brillo. | Propuesta |
| [blending/mix.frag](../bin/data/shaders/blending/mix.frag) | Fundido | Mezclador | composición | Interpola entre dos fuentes. | Propuesta |
| [blending/screen.frag](../bin/data/shaders/blending/screen.frag) | Trama | Mezclador | composición | Combina dos fuentes mediante trama. | Propuesta |
| [blending/multiply.frag](../bin/data/shaders/blending/multiply.frag) | Multiplicar | Mezclador | composición | Multiplica los colores de dos fuentes. | Propuesta |
| [blending/add.frag](../bin/data/shaders/blending/add.frag) | Sumar | Mezclador | composición | Suma los colores de dos fuentes. | Propuesta |
| [blending/average.frag](../bin/data/shaders/blending/average.frag) | Promedio | Mezclador | composición | Promedia dos imágenes de entrada. | Propuesta |
| [blending/difference.frag](../bin/data/shaders/blending/difference.frag) | Diferencia | Mezclador | composición | Muestra diferencias entre colores de dos fuentes. | Propuesta |

## Tanda 1 revisada: generadores de color y patrones

Revisión de código: 2026-09-16. Los ocho son generadores. Nombres y etiquetas acordados con Nico el 2026-09-16; revisión visual pendiente.

| Archivo | Nombre propuesto | Etiquetas | Función observada |
|---|---|---|---|
| `solidcolor.frag` | Color sólido | color | Genera un color RGB uniforme. `mivariable` no interviene en la salida. |
| `degrade.frag` | Patrones de dos colores | color, patrones | Interpola dos colores con patrones lineales, radiales o angulares; permite variar forma de onda, frecuencia y velocidad. |
| `basic2.frag` | Barrido senoidal | color, patrones | Bandas verticales animadas: oscila el canal rojo; verde y azul quedan fijos en 0.5 y 1. Sin controles propios. |
| `simplelines.frag` | Franjas | geometría, patrones | Franjas monocromas horizontales o verticales, con frecuencia, velocidad y umbral de intensidad ajustables. |
| `simplelines_2.0.frag` | Gradiente rojo-verde (experimental) | color | La salida actual es vec4(uv.x, param1, 0, 1). Los cálculos de franjas no llegan a la salida; no es una segunda versión funcional de Franjas. |
| `grid.frag` | Grilla de polígonos | geometría, patrones | Repite polígonos con cantidad de lados, escala, desplazamiento y rotación ajustables; suma feedback según feedbackst. |
| `quad.frag` | Cuadrado pulsante | geometría | Polígono de cuatro lados con desplazamiento, escala por eje y pulsación de tamaño. Lee feedback pero no lo incorpora a la salida. |
| `allpaterns.frag` | Mezcla de ruidos | ruido, patrones | Interpola siete campos procedurales, entre ellos ruido, Voronoi y fBm; permite escala y desplazamiento animado por eje. |

### Acuerdos y pendientes de esta tanda

- Acordados los ocho nombres visibles de la tabla, incluidos «Patrones de dos colores» y «Mezcla de ruidos».
- `simplelines_2.0`: acordado conservarlo como «Gradiente rojo-verde (experimental)», en Generador / color. Su inclusión en la colección final sigue pendiente.
- `grid`: generador con capacidad de feedback; verificar visualmente las estelas. El feedback no cambia su categoría principal.
- `quad`: nombre «Cuadrado pulsante» acordado; verificar visualmente la orientación y el aspecto de la forma.
- `basic2`: decidir luego si aporta como ejemplo sencillo frente a `degrade`, que tiene más controles. No eliminar ninguno durante la clasificación.

Solo se actualizó este documento; no se corrigieron parámetros ni shaders.

## Tanda 2: anillos, círculo y túnel

Agregada al modo curado para probar. Nombres y etiquetas propuestos tras revisar el código; pendientes de evaluación del usuario.

| Archivo | Nombre propuesto | Etiquetas |
|---|---|---|
| `anillos3D.frag` | Anillos 3D | geometría, 3D, repetición |
| `circuloresolution.frag` | Círculo suave | geometría |
| `circuloresolution2.frag` | Gradiente XY (experimental) | color |
| `tunel.frag` | Túnel | geometría, 3D, repetición |

Observaciones a comprobar durante la prueba:

- **Anillos 3D:** `rotx1`/`roty1` orientan la vista; `size1`, `tile1` y `gordura` cambian tamaño, separación y grosor. Revisar fluidez; calcula geometría 3D mediante ray marching.
- **Círculo suave:** `posx`/`posy`, `size` y `sizedif` controlan posición, radio y borde. La corrección de proporción está fijada a 1920/1080: comprobar deformación al cambiar la relación de aspecto.
- **Gradiente XY (experimental):** la salida es `vec4(uv.x, uv.y, 1.0, 1.0)`; `size` y `rojo1` no la afectan. Se clasifica por su salida actual, no por el nombre del archivo.
- **Túnel:** `movspeed` controla el avance, `radial2` el radio y los dos grupos RGB la iluminación. Revisar fluidez y colores.

Solo se agregaron fichas al JSON de curado y al documento. No se modificaron los `.frag`.

## Tanda 3: efectos y mezcladores pedidos por Nico

Incorporados a curado por pedido explícito. Rutas y entradas revisadas en código; prueba visual pendiente.

| Pedido | Archivo | Nombre visible | Categoría | Entradas |
|---|---|---|---|---|
| mirror | `imageprocessing/mirrorquad.frag` | Espejo | Efecto | textura1 |
| transform | `imageprocessing/transform.frag` | Transformar | Efecto | textura1 |
| kalideoscope | `imageprocessing/kaleidoscope.frag` | Caleidoscopio | Efecto | textura1 |
| rotatecolor | `imageprocessing/rotatecolor.frag` | Rotación de color RGB | Efecto | tx |
| chromadist | `blending/chromadist.frag` | Mezcla por croma | Mezclador | textura1, textura2 |
| allblendingmode | `blending/allblendingmodesinoneshader.frag` | Modos de mezcla | Mezclador | textura1, textura2 |

`mirror` corresponde a `mirrorquad.frag`, `kalideoscope` a `kaleidoscope.frag` y `allblendingmode` a `allblendingmodesinoneshader.frag`.
`rotatecolor` rota pares de canales RGB; es distinto de `huerotate`.
`chromadist` combina dos fuentes por distancia a un color, por eso se clasifica como mezclador.
Se mantienen los archivos y parámetros originales. No se modificaron `.frag` ni `.xml`.

## Inventario completo

461 archivos: 449 con punto de entrada fuera de `private` y 12 auxiliares/internos según inspección estática. No certifica compilación ni funcionamiento.

Categorías tentativas según carpeta actual. Las entradas son samplers declarados detectados por el parser, excluyendo los internos y `feedback`; no garantizan uso efectivo ni incluyen necesariamente declaraciones de archivos incluidos. Los archivos de otras carpetas quedan Por definir. Ninguna fila implica exclusión de la organización por su procedencia o estado técnico.

### shaders (1)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [common.frag](../bin/data/shaders/common.frag) | Auxiliar / interno | — | Uso interno; Declara feedback |
### shaders/blending (42)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [blending/add.frag](../bin/data/shaders/blending/add.frag) | Mezclador | textura1, textura2, textura3 | Pendiente de revisión funcional y visual |
| [blending/addtextures.frag](../bin/data/shaders/blending/addtextures.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/allblendingmodesinoneshader.frag](../bin/data/shaders/blending/allblendingmodesinoneshader.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/average.frag](../bin/data/shaders/blending/average.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/blendstich.frag](../bin/data/shaders/blending/blendstich.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/chromablend.frag](../bin/data/shaders/blending/chromablend.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/chromadist.frag](../bin/data/shaders/blending/chromadist.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/colorburn.frag](../bin/data/shaders/blending/colorburn.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/colordodge.frag](../bin/data/shaders/blending/colordodge.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/darken.frag](../bin/data/shaders/blending/darken.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/diference.frag](../bin/data/shaders/blending/diference.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/difference.frag](../bin/data/shaders/blending/difference.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/exlusion.frag](../bin/data/shaders/blending/exlusion.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/glow.frag](../bin/data/shaders/blending/glow.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/hardlight.frag](../bin/data/shaders/blending/hardlight.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/hardmix.frag](../bin/data/shaders/blending/hardmix.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/hueblend.frag](../bin/data/shaders/blending/hueblend.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/lightnen.frag](../bin/data/shaders/blending/lightnen.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/linearburn.frag](../bin/data/shaders/blending/linearburn.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/lineardodge.frag](../bin/data/shaders/blending/lineardodge.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/linearlight.frag](../bin/data/shaders/blending/linearlight.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/mask.frag](../bin/data/shaders/blending/mask.frag) | Mezclador | textura1, textura2, textura3 | Pendiente de revisión funcional y visual |
| [blending/mix.frag](../bin/data/shaders/blending/mix.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/mixdisplace.frag](../bin/data/shaders/blending/mixdisplace.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/mixdisplace2.frag](../bin/data/shaders/blending/mixdisplace2.frag) | Mezclador | texture1, texture2 | Pendiente de revisión funcional y visual |
| [blending/mixdisplacescalefalopa.frag](../bin/data/shaders/blending/mixdisplacescalefalopa.frag) | Mezclador | texture1, texture2 | Pendiente de revisión funcional y visual |
| [blending/mixonmix.frag](../bin/data/shaders/blending/mixonmix.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/mixonstep.frag](../bin/data/shaders/blending/mixonstep.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/multiply.frag](../bin/data/shaders/blending/multiply.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/multiplytextures.frag](../bin/data/shaders/blending/multiplytextures.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/negation.frag](../bin/data/shaders/blending/negation.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/normal.frag](../bin/data/shaders/blending/normal.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/overlay.frag](../bin/data/shaders/blending/overlay.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/overlay2.frag](../bin/data/shaders/blending/overlay2.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/phoenix.frag](../bin/data/shaders/blending/phoenix.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/pinlight.frag](../bin/data/shaders/blending/pinlight.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/scaledisplace.frag](../bin/data/shaders/blending/scaledisplace.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/screen.frag](../bin/data/shaders/blending/screen.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/softlight.frag](../bin/data/shaders/blending/softlight.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/stepx.frag](../bin/data/shaders/blending/stepx.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/substracttextures.frag](../bin/data/shaders/blending/substracttextures.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
| [blending/vividlight.frag](../bin/data/shaders/blending/vividlight.frag) | Mezclador | textura1, textura2 | Pendiente de revisión funcional y visual |
### shaders/combo (5)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [combo/ksetmap.frag](../bin/data/shaders/combo/ksetmap.frag) | Por definir | input_image | Pendiente de revisión funcional y visual |
| [combo/magicpencil.frag](../bin/data/shaders/combo/magicpencil.frag) | Por definir | iChannel0, iChannel1 | Pendiente de revisión funcional y visual |
| [combo/planedisplace.frag](../bin/data/shaders/combo/planedisplace.frag) | Por definir | textura | Pendiente de revisión funcional y visual |
| [combo/raymarching2.frag](../bin/data/shaders/combo/raymarching2.frag) | Por definir | background, sph_texture, floor_texture | Pendiente de revisión funcional y visual |
| [combo/sinparticles_v2.5.frag](../bin/data/shaders/combo/sinparticles_v2.5.frag) | Por definir | col1, col2 | Pendiente de revisión funcional y visual |
### shaders/contrib (95)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [contrib/172018.frag](../bin/data/shaders/contrib/172018.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/Proteanclouds.frag](../bin/data/shaders/contrib/Proteanclouds.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/TorusJourney.frag](../bin/data/shaders/contrib/TorusJourney.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/abandonedconstruction.frag](../bin/data/shaders/contrib/abandonedconstruction.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/abstractterrainobjets.frag](../bin/data/shaders/contrib/abstractterrainobjets.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/alteredjuliajungle.frag](../bin/data/shaders/contrib/alteredjuliajungle.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/anothersynthwavesunsetthing.frag](../bin/data/shaders/contrib/anothersynthwavesunsetthing.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/apollonian.frag](../bin/data/shaders/contrib/apollonian.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/attackflower.frag](../bin/data/shaders/contrib/attackflower.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/boredcircuit.frag](../bin/data/shaders/contrib/boredcircuit.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/butterflyintheskies.frag](../bin/data/shaders/contrib/butterflyintheskies.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/cablevortex2.frag](../bin/data/shaders/contrib/cablevortex2.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/comicblobs.frag](../bin/data/shaders/contrib/comicblobs.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/corridortravel.frag](../bin/data/shaders/contrib/corridortravel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/cubesaredancing.frag](../bin/data/shaders/contrib/cubesaredancing.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/cyberfuji2020.frag](../bin/data/shaders/contrib/cyberfuji2020.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/cylindricallymappedhexagons.frag](../bin/data/shaders/contrib/cylindricallymappedhexagons.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/day43.frag.frag](../bin/data/shaders/contrib/day43.frag.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/deconstructivecube.frag](../bin/data/shaders/contrib/deconstructivecube.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/discostartunnel.frag](../bin/data/shaders/contrib/discostartunnel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/doublehelix.frag](../bin/data/shaders/contrib/doublehelix.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/dropofdistortion.frag.frag](../bin/data/shaders/contrib/dropofdistortion.frag.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/effusinglava.frag](../bin/data/shaders/contrib/effusinglava.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/extrudedmobiusspiral.frag](../bin/data/shaders/contrib/extrudedmobiusspiral.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/featherswithsynthwavecolors.frag](../bin/data/shaders/contrib/featherswithsynthwavecolors.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/fishingvillagev2.frag](../bin/data/shaders/contrib/fishingvillagev2.frag) | Por definir | iChannel0, iChannel1, iChannel2 | Pendiente de revisión funcional y visual |
| [contrib/floatingcolorfulspheres.frag](../bin/data/shaders/contrib/floatingcolorfulspheres.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/flyingintodigitaltunnel.frag](../bin/data/shaders/contrib/flyingintodigitaltunnel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/fractalpyramid.frag](../bin/data/shaders/contrib/fractalpyramid.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/gennoisewindows.frag](../bin/data/shaders/contrib/gennoisewindows.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/ghostchamber.frag](../bin/data/shaders/contrib/ghostchamber.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/glowingpetals.frag](../bin/data/shaders/contrib/glowingpetals.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/gpudelirium.frag](../bin/data/shaders/contrib/gpudelirium.frag) | Por definir | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/contrib/truchetportals.frag |
| [contrib/heart3d.frag](../bin/data/shaders/contrib/heart3d.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/heartbeat2D.frag](../bin/data/shaders/contrib/heartbeat2D.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/heartwingangel.frag](../bin/data/shaders/contrib/heartwingangel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/hyperspacetunnel.frag](../bin/data/shaders/contrib/hyperspacetunnel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/infiniterepetition.frag](../bin/data/shaders/contrib/infiniterepetition.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/infinitezoomingmap.frag](../bin/data/shaders/contrib/infinitezoomingmap.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/isometricflat.frag](../bin/data/shaders/contrib/isometricflat.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/itsaquestionoftime.frag](../bin/data/shaders/contrib/itsaquestionoftime.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/jellygateways.frag](../bin/data/shaders/contrib/jellygateways.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/jellytubeforest.frag](../bin/data/shaders/contrib/jellytubeforest.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/jungleRaymarching.frag](../bin/data/shaders/contrib/jungleRaymarching.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/kaleidoscopecity.frag](../bin/data/shaders/contrib/kaleidoscopecity.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/kalizyl.frag](../bin/data/shaders/contrib/kalizyl.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/lionsnekmurph.frag](../bin/data/shaders/contrib/lionsnekmurph.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/llamels.frag](../bin/data/shaders/contrib/llamels.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/machineroom.frag](../bin/data/shaders/contrib/machineroom.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/merrychristmas.frag](../bin/data/shaders/contrib/merrychristmas.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/mistbyohnopart2.frag](../bin/data/shaders/contrib/mistbyohnopart2.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/moltenneon.frag](../bin/data/shaders/contrib/moltenneon.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/moonhealingescalation.frag](../bin/data/shaders/contrib/moonhealingescalation.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/morningcity.frag](../bin/data/shaders/contrib/morningcity.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/noise3dflythough.frag](../bin/data/shaders/contrib/noise3dflythough.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notday79.frag](../bin/data/shaders/contrib/notday79.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/numbersincyberspace.frag](../bin/data/shaders/contrib/numbersincyberspace.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/overthemoon.frag](../bin/data/shaders/contrib/overthemoon.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/partyconcertvisuals.frag](../bin/data/shaders/contrib/partyconcertvisuals.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/partyconcertvisuals2020.frag](../bin/data/shaders/contrib/partyconcertvisuals2020.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/pinscreen.frag](../bin/data/shaders/contrib/pinscreen.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/planetarygears.frag](../bin/data/shaders/contrib/planetarygears.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/plexusparticles.frag](../bin/data/shaders/contrib/plexusparticles.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/plexusparticles2.frag](../bin/data/shaders/contrib/plexusparticles2.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/polygonlandscapes.frag](../bin/data/shaders/contrib/polygonlandscapes.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/pridetunnel.frag](../bin/data/shaders/contrib/pridetunnel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/rainbowspaghetti.frag](../bin/data/shaders/contrib/rainbowspaghetti.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/reflectingcrystals.frag](../bin/data/shaders/contrib/reflectingcrystals.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/runningoutoftime.frag](../bin/data/shaders/contrib/runningoutoftime.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/shaderoyale2.frag](../bin/data/shaders/contrib/shaderoyale2.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/shadertober.frag](../bin/data/shaders/contrib/shadertober.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/shiftingarrowsanimation.frag](../bin/data/shaders/contrib/shiftingarrowsanimation.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/shinytoy.frag](../bin/data/shaders/contrib/shinytoy.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/sierpinskiexperiment.frag](../bin/data/shaders/contrib/sierpinskiexperiment.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/sousocean.frag](../bin/data/shaders/contrib/sousocean.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/sparksfromfire.frag](../bin/data/shaders/contrib/sparksfromfire.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/spelldemonsouls.frag](../bin/data/shaders/contrib/spelldemonsouls.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/starnest.frag](../bin/data/shaders/contrib/starnest.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/starwaytothestars.frag](../bin/data/shaders/contrib/starwaytothestars.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/sugarrash.frag](../bin/data/shaders/contrib/sugarrash.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/thesecretplace.frag](../bin/data/shaders/contrib/thesecretplace.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/totherodaofribbon.frag](../bin/data/shaders/contrib/totherodaofribbon.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/trianglevoronoigraphweave.frag](../bin/data/shaders/contrib/trianglevoronoigraphweave.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/triangulatedheightfield.frag](../bin/data/shaders/contrib/triangulatedheightfield.frag) | Por definir | iChannel0, iChannel1 | Pendiente de revisión funcional y visual |
| [contrib/truchetportals.frag](../bin/data/shaders/contrib/truchetportals.frag) | Por definir | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/contrib/gpudelirium.frag |
| [contrib/tunelv4.frag](../bin/data/shaders/contrib/tunelv4.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/vibrantcube.frag](../bin/data/shaders/contrib/vibrantcube.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/vinesatnight.frag](../bin/data/shaders/contrib/vinesatnight.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/voronoinoisegrid.frag](../bin/data/shaders/contrib/voronoinoisegrid.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/voronoivegetation.frag](../bin/data/shaders/contrib/voronoivegetation.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/whitedrops.frag](../bin/data/shaders/contrib/whitedrops.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/windowinwindow.frag](../bin/data/shaders/contrib/windowinwindow.frag) | Por definir | iChannel0, iChannel1 | Pendiente de revisión funcional y visual |
| [contrib/windows95.frag](../bin/data/shaders/contrib/windows95.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/wobblything.frag](../bin/data/shaders/contrib/wobblything.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/worms.frag](../bin/data/shaders/contrib/worms.frag) | Por definir | — | Pendiente de revisión funcional y visual |
### shaders/contrib/halfworking (8)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [contrib/halfworking/intothewoods.frag](../bin/data/shaders/contrib/halfworking/intothewoods.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/halfworking/miraclesnowflakes.frag](../bin/data/shaders/contrib/halfworking/miraclesnowflakes.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/halfworking/nightfall_ANDAMAL.frag](../bin/data/shaders/contrib/halfworking/nightfall_ANDAMAL.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/halfworking/solarflare.frag](../bin/data/shaders/contrib/halfworking/solarflare.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/halfworking/terrainlattice.frag](../bin/data/shaders/contrib/halfworking/terrainlattice.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/halfworking/twistedcolumns.frag](../bin/data/shaders/contrib/halfworking/twistedcolumns.frag) | Por definir | texture1 | Pendiente de revisión funcional y visual |
| [contrib/halfworking/voxelpacman.frag](../bin/data/shaders/contrib/halfworking/voxelpacman.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/halfworking/wormbubbles.frag](../bin/data/shaders/contrib/halfworking/wormbubbles.frag) | Por definir | — | Pendiente de revisión funcional y visual |
### shaders/contrib/notworking (17)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [contrib/notworking/2001spacestation.frag](../bin/data/shaders/contrib/notworking/2001spacestation.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/2Dclouds.frag](../bin/data/shaders/contrib/notworking/2Dclouds.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/abstractcorridor.frag](../bin/data/shaders/contrib/notworking/abstractcorridor.frag) | Por definir | iChannel0, iChannel1 | Pendiente de revisión funcional y visual |
| [contrib/notworking/alienthorns.frag](../bin/data/shaders/contrib/notworking/alienthorns.frag) | Por definir | iChannel0, iChannel1, iChannel2, iChannel3 | Pendiente de revisión funcional y visual |
| [contrib/notworking/bloodbath.frag](../bin/data/shaders/contrib/notworking/bloodbath.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/buildingplustrain_8bits.frag](../bin/data/shaders/contrib/notworking/buildingplustrain_8bits.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/chainsandgears.frag](../bin/data/shaders/contrib/notworking/chainsandgears.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/cityinthesky.frag](../bin/data/shaders/contrib/notworking/cityinthesky.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/flyngcentipede.frag](../bin/data/shaders/contrib/notworking/flyngcentipede.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/fractalflythrough.frag](../bin/data/shaders/contrib/notworking/fractalflythrough.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/notworking/hypertunnel.frag](../bin/data/shaders/contrib/notworking/hypertunnel.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/moviepicture.frag](../bin/data/shaders/contrib/notworking/moviepicture.frag) | Por definir | iChannel1 | Pendiente de revisión funcional y visual |
| [contrib/notworking/planetshadertoy.frag](../bin/data/shaders/contrib/notworking/planetshadertoy.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/radar.frag](../bin/data/shaders/contrib/notworking/radar.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/theeye.frag](../bin/data/shaders/contrib/notworking/theeye.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
| [contrib/notworking/triangulatedheightfieldtrick.frag](../bin/data/shaders/contrib/notworking/triangulatedheightfieldtrick.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [contrib/notworking/tunnelwithlamps.frag](../bin/data/shaders/contrib/notworking/tunnelwithlamps.frag) | Por definir | — | Pendiente de revisión funcional y visual |
### shaders/generative (161)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [generative/3Dmap.frag](../bin/data/shaders/generative/3Dmap.frag) | Generador | intext | Pendiente de revisión funcional y visual |
| [generative/FractLines.frag](../bin/data/shaders/generative/FractLines.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/FractLines2.frag](../bin/data/shaders/generative/FractLines2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/aesteticpolys.frag](../bin/data/shaders/generative/aesteticpolys.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/allpaterns.frag](../bin/data/shaders/generative/allpaterns.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/anillos3D.frag](../bin/data/shaders/generative/anillos3D.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/atomdistort.frag](../bin/data/shaders/generative/atomdistort.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/baby.frag](../bin/data/shaders/generative/baby.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/baby2.frag](../bin/data/shaders/generative/baby2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/baby3.frag](../bin/data/shaders/generative/baby3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/basic.frag](../bin/data/shaders/generative/basic.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/basic2.frag](../bin/data/shaders/generative/basic2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/beach.frag](../bin/data/shaders/generative/beach.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/bicho.frag](../bin/data/shaders/generative/bicho.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/blancoia.frag](../bin/data/shaders/generative/blancoia.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/caleido.frag](../bin/data/shaders/generative/caleido.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/cardinals.frag](../bin/data/shaders/generative/cardinals.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuits.frag](../bin/data/shaders/generative/circuits.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circularraymarching.frag](../bin/data/shaders/generative/circularraymarching.frag) | Generador | fondo | Pendiente de revisión funcional y visual |
| [generative/circulofeedback.frag](../bin/data/shaders/generative/circulofeedback.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuloresolution.frag](../bin/data/shaders/generative/circuloresolution.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuloresolution2.frag](../bin/data/shaders/generative/circuloresolution2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuloresolution_doble.frag](../bin/data/shaders/generative/circuloresolution_doble.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuloresolution_feedback.frag](../bin/data/shaders/generative/circuloresolution_feedback.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circuloresolutionprueba.frag](../bin/data/shaders/generative/circuloresolutionprueba.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/circulos_chela.frag](../bin/data/shaders/generative/circulos_chela.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/circulotommy.frag |
| [generative/circulotommy.frag](../bin/data/shaders/generative/circulotommy.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/circulos_chela.frag |
| [generative/cocacoca.frag](../bin/data/shaders/generative/cocacoca.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/cocacoca2.frag](../bin/data/shaders/generative/cocacoca2.frag) | Generador | fondo, ce | Pendiente de revisión funcional y visual |
| [generative/coliflor.frag](../bin/data/shaders/generative/coliflor.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/coliflor2.frag](../bin/data/shaders/generative/coliflor2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/colorin.frag](../bin/data/shaders/generative/colorin.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/construituciudad.frag](../bin/data/shaders/generative/construituciudad.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/crazyraymarchingsphere.frag](../bin/data/shaders/generative/crazyraymarchingsphere.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/crazyraymarchingsphere2.frag](../bin/data/shaders/generative/crazyraymarchingsphere2.frag) | Generador | background, sph_texture, floor_texture | Pendiente de revisión funcional y visual |
| [generative/cubosesfera1.frag](../bin/data/shaders/generative/cubosesfera1.frag) | Generador | t1, t2 | Pendiente de revisión funcional y visual |
| [generative/degrade.frag](../bin/data/shaders/generative/degrade.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/demonic.frag](../bin/data/shaders/generative/demonic.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/demonic_fxhash.frag](../bin/data/shaders/generative/demonic_fxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/dientes.frag](../bin/data/shaders/generative/dientes.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/dragon2.frag](../bin/data/shaders/generative/dragon2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/dragon3.frag](../bin/data/shaders/generative/dragon3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/dragon4.frag](../bin/data/shaders/generative/dragon4.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/dungeoncritter.frag](../bin/data/shaders/generative/dungeoncritter.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/eclipse.frag](../bin/data/shaders/generative/eclipse.frag) | Generador | tex | Pendiente de revisión funcional y visual |
| [generative/eclipsefalopa.frag](../bin/data/shaders/generative/eclipsefalopa.frag) | Generador | tex | Pendiente de revisión funcional y visual |
| [generative/emerging.frag](../bin/data/shaders/generative/emerging.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/energizer.frag](../bin/data/shaders/generative/energizer.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/esterioscopiaPV.frag](../bin/data/shaders/generative/esterioscopiaPV.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/ethereal.frag](../bin/data/shaders/generative/ethereal.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/fbm.frag](../bin/data/shaders/generative/fbm.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/fbmvuse.frag](../bin/data/shaders/generative/fbmvuse.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/feedbackballs.frag](../bin/data/shaders/generative/feedbackballs.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/feelingeldorado.frag](../bin/data/shaders/generative/feelingeldorado.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/fractalland.frag](../bin/data/shaders/generative/fractalland.frag) | Generador | tx | Pendiente de revisión funcional y visual |
| [generative/grid.frag](../bin/data/shaders/generative/grid.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/grid2.frag](../bin/data/shaders/generative/grid2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/helados.frag](../bin/data/shaders/generative/helados.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/helados_mod1.frag](../bin/data/shaders/generative/helados_mod1.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/helados_mod2.frag](../bin/data/shaders/generative/helados_mod2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/helados_mod3..frag](../bin/data/shaders/generative/helados_mod3..frag) | Generador | texture1 | Pendiente de revisión funcional y visual |
| [generative/helados_mod4.frag](../bin/data/shaders/generative/helados_mod4.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/heroevj.frag](../bin/data/shaders/generative/heroevj.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/hippielava.frag](../bin/data/shaders/generative/hippielava.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/humanmeatgrinder.frag](../bin/data/shaders/generative/humanmeatgrinder.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/humanmeatgrinder_fxhash.frag](../bin/data/shaders/generative/humanmeatgrinder_fxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/infection.frag](../bin/data/shaders/generative/infection.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/inversionmachine.frag](../bin/data/shaders/generative/inversionmachine.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/jelly.frag](../bin/data/shaders/generative/jelly.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/jupiter.frag](../bin/data/shaders/generative/jupiter.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/jupiter2.frag](../bin/data/shaders/generative/jupiter2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/jupiter3.frag](../bin/data/shaders/generative/jupiter3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/klingon.frag](../bin/data/shaders/generative/klingon.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/klingonfxhash.frag](../bin/data/shaders/generative/klingonfxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/kset1.frag](../bin/data/shaders/generative/kset1.frag) | Auxiliar / interno | — | Uso interno |
| [generative/kset2.frag](../bin/data/shaders/generative/kset2.frag) | Auxiliar / interno | — | Uso interno |
| [generative/ksetmap.frag](../bin/data/shaders/generative/ksetmap.frag) | Generador | input_image | Pendiente de revisión funcional y visual |
| [generative/ksetraps.frag](../bin/data/shaders/generative/ksetraps.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/ksetraps2.frag](../bin/data/shaders/generative/ksetraps2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/lineasrojas.frag](../bin/data/shaders/generative/lineasrojas.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/lineasrojas2.frag |
| [generative/lineasrojas2.frag](../bin/data/shaders/generative/lineasrojas2.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/lineasrojas.frag |
| [generative/lineasrojas3.frag](../bin/data/shaders/generative/lineasrojas3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/lunafns.frag](../bin/data/shaders/generative/lunafns.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/luzysombra.frag](../bin/data/shaders/generative/luzysombra.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/metioritosfalopa.frag](../bin/data/shaders/generative/metioritosfalopa.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/metro.frag](../bin/data/shaders/generative/metro.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/miprimerrandom.frag](../bin/data/shaders/generative/miprimerrandom.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/moco.frag](../bin/data/shaders/generative/moco.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/negroia.frag](../bin/data/shaders/generative/negroia.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/noiseparty2.frag](../bin/data/shaders/generative/noiseparty2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/noisepartyfxhash.frag](../bin/data/shaders/generative/noisepartyfxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/nubes.frag](../bin/data/shaders/generative/nubes.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/nubes_sanjuan.frag](../bin/data/shaders/generative/nubes_sanjuan.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/p5d.frag](../bin/data/shaders/generative/p5d.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/p5d_#.frag |
| [generative/p5d_#.frag](../bin/data/shaders/generative/p5d_#.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/p5d.frag |
| [generative/p5d_2.frag](../bin/data/shaders/generative/p5d_2.frag) | Generador | t1, t2 | Pendiente de revisión funcional y visual |
| [generative/p5d_3.frag](../bin/data/shaders/generative/p5d_3.frag) | Generador | t1, t2 | Pendiente de revisión funcional y visual |
| [generative/palos.frag](../bin/data/shaders/generative/palos.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/pamilo.frag](../bin/data/shaders/generative/pamilo.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/particulascubos.frag](../bin/data/shaders/generative/particulascubos.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/particulascubos_fxhash.frag |
| [generative/particulascubos_fxhash.frag](../bin/data/shaders/generative/particulascubos_fxhash.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/particulascubos.frag |
| [generative/perlinnoisefires.frag](../bin/data/shaders/generative/perlinnoisefires.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/piinteractivo.frag](../bin/data/shaders/generative/piinteractivo.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/planetchild.frag](../bin/data/shaders/generative/planetchild.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/polysuperfractal.frag](../bin/data/shaders/generative/polysuperfractal.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/poster.frag](../bin/data/shaders/generative/poster.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/poster_fxhash.frag](../bin/data/shaders/generative/poster_fxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/prueba1.frag](../bin/data/shaders/generative/prueba1.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/quad.frag](../bin/data/shaders/generative/quad.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/radialmadness.frag](../bin/data/shaders/generative/radialmadness.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/random.frag](../bin/data/shaders/generative/random.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/randomaxxion.frag](../bin/data/shaders/generative/randomaxxion.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/randomfxhash.frag](../bin/data/shaders/generative/randomfxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rayshader.frag](../bin/data/shaders/generative/rayshader.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rbstar.frag](../bin/data/shaders/generative/rbstar.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rbstar2.frag](../bin/data/shaders/generative/rbstar2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rbstar3_falop.frag](../bin/data/shaders/generative/rbstar3_falop.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rbstarfxhash.frag](../bin/data/shaders/generative/rbstarfxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/redbullstarnestv3.frag](../bin/data/shaders/generative/redbullstarnestv3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/redlines (3).frag](../bin/data/shaders/generative/redlines%20(3).frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/redlines.frag](../bin/data/shaders/generative/redlines.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rmtest2.frag](../bin/data/shaders/generative/rmtest2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rxr.frag](../bin/data/shaders/generative/rxr.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rxrpure - Copy.frag](../bin/data/shaders/generative/rxrpure%20-%20Copy.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rxrpure.frag](../bin/data/shaders/generative/rxrpure.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/rxrsun.frag](../bin/data/shaders/generative/rxrsun.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/santi1.frag](../bin/data/shaders/generative/santi1.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/simplelines.frag](../bin/data/shaders/generative/simplelines.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/simplelines_2.0.frag](../bin/data/shaders/generative/simplelines_2.0.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/sinparticles_fxhash.frag](../bin/data/shaders/generative/sinparticles_fxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/sinparticles_v2.5.frag](../bin/data/shaders/generative/sinparticles_v2.5.frag) | Generador | col1, col2 | Pendiente de revisión funcional y visual |
| [generative/sinparticles_v3.frag](../bin/data/shaders/generative/sinparticles_v3.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/sol.frag](../bin/data/shaders/generative/sol.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/solfns.frag](../bin/data/shaders/generative/solfns.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/solidcolor.frag](../bin/data/shaders/generative/solidcolor.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/solidcolor2.frag](../bin/data/shaders/generative/solidcolor2.frag) | Generador | f1 | Pendiente de revisión funcional y visual |
| [generative/sphereextrude.frag](../bin/data/shaders/generative/sphereextrude.frag) | Generador | tx | Pendiente de revisión funcional y visual |
| [generative/synthwavecrc_v4.frag](../bin/data/shaders/generative/synthwavecrc_v4.frag) | Generador | tx1, tx2 | Pendiente de revisión funcional y visual |
| [generative/synthwavecrc_v5.frag](../bin/data/shaders/generative/synthwavecrc_v5.frag) | Generador | tx1, tx2 | Pendiente de revisión funcional y visual |
| [generative/synthwavecrc_v6.frag](../bin/data/shaders/generative/synthwavecrc_v6.frag) | Generador | tx1, tx2 | Pendiente de revisión funcional y visual |
| [generative/synthwavecrc_v7.frag](../bin/data/shaders/generative/synthwavecrc_v7.frag) | Generador | tx1, tx2 | Pendiente de revisión funcional y visual |
| [generative/synthwavecrc_v8_solofondo.frag](../bin/data/shaders/generative/synthwavecrc_v8_solofondo.frag) | Generador | tx1, tx2 | Pendiente de revisión funcional y visual |
| [generative/tecnoneon.frag](../bin/data/shaders/generative/tecnoneon.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/template.frag](../bin/data/shaders/generative/template.frag) | Generador | koko | Pendiente de revisión funcional y visual |
| [generative/template2.frag](../bin/data/shaders/generative/template2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/test.frag](../bin/data/shaders/generative/test.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/thestartofsomethingbeautiful.frag](../bin/data/shaders/generative/thestartofsomethingbeautiful.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/torsionengines.frag](../bin/data/shaders/generative/torsionengines.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/tunel.frag](../bin/data/shaders/generative/tunel.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/tunel3.frag |
| [generative/tunel2.frag](../bin/data/shaders/generative/tunel2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/tunel3.frag](../bin/data/shaders/generative/tunel3.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/tunel.frag |
| [generative/videosinte.frag](../bin/data/shaders/generative/videosinte.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/videosintevuse.frag |
| [generative/videosintefxhash.frag](../bin/data/shaders/generative/videosintefxhash.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/videosintevuse.frag](../bin/data/shaders/generative/videosintevuse.frag) | Generador | — | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/videosinte.frag |
| [generative/violetaia.frag](../bin/data/shaders/generative/violetaia.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/voronoiinvoronoi.frag](../bin/data/shaders/generative/voronoiinvoronoi.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/voronoinoisegrid2.frag](../bin/data/shaders/generative/voronoinoisegrid2.frag) | Generador | — | Pendiente de revisión funcional y visual |
| [generative/water - Copy.frag](../bin/data/shaders/generative/water%20-%20Copy.frag) | Generador | tex | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/water.frag |
| [generative/water.frag](../bin/data/shaders/generative/water.frag) | Generador | tex | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/water - Copy.frag |
| [generative/water2.frag](../bin/data/shaders/generative/water2.frag) | Generador | tex | Pendiente de revisión funcional y visual |
| [generative/xraymarching.frag](../bin/data/shaders/generative/xraymarching.frag) | Generador | — | Pendiente de revisión funcional y visual |
### shaders/generative/bckup (2)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [generative/bckup/lunafns.frag](../bin/data/shaders/generative/bckup/lunafns.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [generative/bckup/solfns.frag](../bin/data/shaders/generative/bckup/solfns.frag) | Por definir | — | Pendiente de revisión funcional y visual |
### shaders/generative/deprecated (11)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [generative/deprecated/eclipse.frag](../bin/data/shaders/generative/deprecated/eclipse.frag) | Por definir | tex | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/deprecated/eclipse_3.frag |
| [generative/deprecated/eclipse2.frag](../bin/data/shaders/generative/deprecated/eclipse2.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/eclipse_3.frag](../bin/data/shaders/generative/deprecated/eclipse_3.frag) | Por definir | tex | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/generative/deprecated/eclipse.frag |
| [generative/deprecated/eclipse_ulti.frag](../bin/data/shaders/generative/deprecated/eclipse_ulti.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/fbm.frag](../bin/data/shaders/generative/deprecated/fbm.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [generative/deprecated/water.frag](../bin/data/shaders/generative/deprecated/water.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/water2.frag](../bin/data/shaders/generative/deprecated/water2.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/water3.frag](../bin/data/shaders/generative/deprecated/water3.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/water5.frag](../bin/data/shaders/generative/deprecated/water5.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/waves.frag](../bin/data/shaders/generative/deprecated/waves.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
| [generative/deprecated/waves2.frag](../bin/data/shaders/generative/deprecated/waves2.frag) | Por definir | tex | Pendiente de revisión funcional y visual |
### shaders/generative/halfworking (3)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [generative/halfworking/klaklak.frag](../bin/data/shaders/generative/halfworking/klaklak.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [generative/halfworking/raymarching0.frag](../bin/data/shaders/generative/halfworking/raymarching0.frag) | Por definir | — | Pendiente de revisión funcional y visual |
| [generative/halfworking/shadertoy.frag](../bin/data/shaders/generative/halfworking/shadertoy.frag) | Por definir | iChannel0 | Pendiente de revisión funcional y visual |
### shaders/imageprocessing (104)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [imageprocessing/VHStapeeffect.frag](../bin/data/shaders/imageprocessing/VHStapeeffect.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/VHStapeeffect2.frag](../bin/data/shaders/imageprocessing/VHStapeeffect2.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/alphaops.frag](../bin/data/shaders/imageprocessing/alphaops.frag) | Efecto | textura1, textura2 | Pendiente de revisión funcional y visual |
| [imageprocessing/asciishader.frag](../bin/data/shaders/imageprocessing/asciishader.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/autodisplace.frag](../bin/data/shaders/imageprocessing/autodisplace.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/bayerdither.frag](../bin/data/shaders/imageprocessing/bayerdither.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/blackandwhite.frag](../bin/data/shaders/imageprocessing/blackandwhite.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/bloom.frag](../bin/data/shaders/imageprocessing/bloom.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/bloom2.frag](../bin/data/shaders/imageprocessing/bloom2.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/bloom3.frag](../bin/data/shaders/imageprocessing/bloom3.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/bloom_chroma.frag](../bin/data/shaders/imageprocessing/bloom_chroma.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/brightcontrast.frag](../bin/data/shaders/imageprocessing/brightcontrast.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/cgamadness.frag](../bin/data/shaders/imageprocessing/cgamadness.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/changefase.frag](../bin/data/shaders/imageprocessing/changefase.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/chromaalpha.frag](../bin/data/shaders/imageprocessing/chromaalpha.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/chromabw.frag](../bin/data/shaders/imageprocessing/chromabw.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/chromakey.frag](../bin/data/shaders/imageprocessing/chromakey.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/colorkey.frag](../bin/data/shaders/imageprocessing/colorkey.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/composite.frag](../bin/data/shaders/imageprocessing/composite.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/crttube.frag](../bin/data/shaders/imageprocessing/crttube.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/darknessrays.frag](../bin/data/shaders/imageprocessing/darknessrays.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/displace3d.frag](../bin/data/shaders/imageprocessing/displace3d.frag) | Efecto | textura | Pendiente de revisión funcional y visual |
| [imageprocessing/displacescalefalopa.frag](../bin/data/shaders/imageprocessing/displacescalefalopa.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/edges.frag](../bin/data/shaders/imageprocessing/edges.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/edges_tofunction.frag](../bin/data/shaders/imageprocessing/edges_tofunction.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/emboss.frag](../bin/data/shaders/imageprocessing/emboss.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/extruder.frag](../bin/data/shaders/imageprocessing/extruder.frag) | Efecto | tx | Pendiente de revisión funcional y visual |
| [imageprocessing/fbfalopa.frag](../bin/data/shaders/imageprocessing/fbfalopa.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedback_advance.frag](../bin/data/shaders/imageprocessing/feedback_advance.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/feedback_advance3.frag |
| [imageprocessing/feedback_advance3.frag](../bin/data/shaders/imageprocessing/feedback_advance3.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/feedback_advance.frag |
| [imageprocessing/feedbackadd.frag](../bin/data/shaders/imageprocessing/feedbackadd.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbackadd_advance.frag](../bin/data/shaders/imageprocessing/feedbackadd_advance.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimit.frag](../bin/data/shaders/imageprocessing/feedbacklimit.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimit2.frag](../bin/data/shaders/imageprocessing/feedbacklimit2.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimit_chroma.frag](../bin/data/shaders/imageprocessing/feedbacklimit_chroma.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace2.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace2.frag) | Efecto | texture1, texture2 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace4.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace4.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace5.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace5.frag) | Efecto | texture1, texture2 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace_chroma.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace_chroma.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbacklimitdisplace_test.frag](../bin/data/shaders/imageprocessing/feedbacklimitdisplace_test.frag) | Efecto | texture1, texture2 | Pendiente de revisión funcional y visual |
| [imageprocessing/feedbackmix.frag](../bin/data/shaders/imageprocessing/feedbackmix.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/fissure.frag](../bin/data/shaders/imageprocessing/fissure.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/flip.frag](../bin/data/shaders/imageprocessing/flip.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/fract.frag](../bin/data/shaders/imageprocessing/fract.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/fract2.frag](../bin/data/shaders/imageprocessing/fract2.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/gameboyfy.frag](../bin/data/shaders/imageprocessing/gameboyfy.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/gameboyfy2.frag](../bin/data/shaders/imageprocessing/gameboyfy2.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/gloss.frag](../bin/data/shaders/imageprocessing/gloss.frag) | Efecto | imagen | Pendiente de revisión funcional y visual |
| [imageprocessing/halftone.frag](../bin/data/shaders/imageprocessing/halftone.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/huerotate.frag](../bin/data/shaders/imageprocessing/huerotate.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/invert.frag](../bin/data/shaders/imageprocessing/invert.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/kaleidocopiar.frag](../bin/data/shaders/imageprocessing/kaleidocopiar.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/kaleidoscope.frag](../bin/data/shaders/imageprocessing/kaleidoscope.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/kindofmotionblur.frag](../bin/data/shaders/imageprocessing/kindofmotionblur.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/lighting3D.frag](../bin/data/shaders/imageprocessing/lighting3D.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/limitanddec.frag](../bin/data/shaders/imageprocessing/limitanddec.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/lumakey.frag](../bin/data/shaders/imageprocessing/lumakey.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/mapping.frag](../bin/data/shaders/imageprocessing/mapping.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/mapping_advanced.frag](../bin/data/shaders/imageprocessing/mapping_advanced.frag) | Efecto | textura1, textura2, textura3, textura4 | Pendiente de revisión funcional y visual |
| [imageprocessing/mapping_debug.frag](../bin/data/shaders/imageprocessing/mapping_debug.frag) | Efecto | — | Pendiente de revisión funcional y visual |
| [imageprocessing/matrixiterations.frag](../bin/data/shaders/imageprocessing/matrixiterations.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/matrixiterations2_colorbuffers.frag](../bin/data/shaders/imageprocessing/matrixiterations2_colorbuffers.frag) | Efecto | textura1, textura2, textura3 | Pendiente de revisión funcional y visual |
| [imageprocessing/matrixiterations3_backup.frag](../bin/data/shaders/imageprocessing/matrixiterations3_backup.frag) | Efecto | textura1, textura2, textura3 | Pendiente de revisión funcional y visual |
| [imageprocessing/mirrorquad.frag](../bin/data/shaders/imageprocessing/mirrorquad.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/mod1.frag](../bin/data/shaders/imageprocessing/mod1.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/mod2.frag](../bin/data/shaders/imageprocessing/mod2.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/multiply.frag](../bin/data/shaders/imageprocessing/multiply.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/noisepartysimage.frag](../bin/data/shaders/imageprocessing/noisepartysimage.frag) | Efecto | tx | Pendiente de revisión funcional y visual |
| [imageprocessing/oldvideo.frag](../bin/data/shaders/imageprocessing/oldvideo.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/perspective.frag](../bin/data/shaders/imageprocessing/perspective.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/planedisplace.frag](../bin/data/shaders/imageprocessing/planedisplace.frag) | Efecto | textura | Pendiente de revisión funcional y visual |
| [imageprocessing/radialblur_advance.frag](../bin/data/shaders/imageprocessing/radialblur_advance.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/radialblur_dof.frag](../bin/data/shaders/imageprocessing/radialblur_dof.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/radialcga.frag](../bin/data/shaders/imageprocessing/radialcga.frag) | Efecto | input_texture | Pendiente de revisión funcional y visual |
| [imageprocessing/radialinversion.frag](../bin/data/shaders/imageprocessing/radialinversion.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/rage.frag](../bin/data/shaders/imageprocessing/rage.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/randomdither.frag](../bin/data/shaders/imageprocessing/randomdither.frag) | Efecto | texture1 | Pendiente de revisión funcional y visual |
| [imageprocessing/raymarchingclase8.frag](../bin/data/shaders/imageprocessing/raymarchingclase8.frag) | Efecto | fondo | Pendiente de revisión funcional y visual |
| [imageprocessing/raymarchingclase82.frag](../bin/data/shaders/imageprocessing/raymarchingclase82.frag) | Efecto | fondo | Pendiente de revisión funcional y visual |
| [imageprocessing/raymarchingclase9.frag](../bin/data/shaders/imageprocessing/raymarchingclase9.frag) | Efecto | fondo | Pendiente de revisión funcional y visual |
| [imageprocessing/raymarchingclase92.frag](../bin/data/shaders/imageprocessing/raymarchingclase92.frag) | Efecto | fondo | Pendiente de revisión funcional y visual |
| [imageprocessing/rays.frag](../bin/data/shaders/imageprocessing/rays.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/recolor.frag](../bin/data/shaders/imageprocessing/recolor.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/rompio.frag](../bin/data/shaders/imageprocessing/rompio.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/rotate.frag](../bin/data/shaders/imageprocessing/rotate.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/rotatecolor.frag](../bin/data/shaders/imageprocessing/rotatecolor.frag) | Efecto | tx | Pendiente de revisión funcional y visual |
| [imageprocessing/rotateiterations.frag](../bin/data/shaders/imageprocessing/rotateiterations.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/saturationbrightness.frag](../bin/data/shaders/imageprocessing/saturationbrightness.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/scale.frag](../bin/data/shaders/imageprocessing/scale.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/shapematte.frag](../bin/data/shaders/imageprocessing/shapematte.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/signalfault.frag](../bin/data/shaders/imageprocessing/signalfault.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/signalwarp.frag](../bin/data/shaders/imageprocessing/signalwarp.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/simplevaporwavefilter.frag](../bin/data/shaders/imageprocessing/simplevaporwavefilter.frag) | Efecto | iChannel0 | Pendiente de revisión funcional y visual |
| [imageprocessing/sinfy.frag](../bin/data/shaders/imageprocessing/sinfy.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/sinfy2.frag](../bin/data/shaders/imageprocessing/sinfy2.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/slitscan.frag](../bin/data/shaders/imageprocessing/slitscan.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/smoothstep - Copy.frag](../bin/data/shaders/imageprocessing/smoothstep%20-%20Copy.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/smoothstep.frag |
| [imageprocessing/smoothstep.frag](../bin/data/shaders/imageprocessing/smoothstep.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/smoothstep - Copy.frag |
| [imageprocessing/sphereextrude.frag](../bin/data/shaders/imageprocessing/sphereextrude.frag) | Efecto | tx | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/sphereextrude3.frag |
| [imageprocessing/sphereextrude2.frag](../bin/data/shaders/imageprocessing/sphereextrude2.frag) | Efecto | tx | Pendiente de revisión funcional y visual |
| [imageprocessing/sphereextrude3.frag](../bin/data/shaders/imageprocessing/sphereextrude3.frag) | Efecto | tx | Pendiente de revisión funcional y visual; Duplicado exacto: shaders/imageprocessing/sphereextrude.frag |
| [imageprocessing/transform.frag](../bin/data/shaders/imageprocessing/transform.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/vignette.frag](../bin/data/shaders/imageprocessing/vignette.frag) | Efecto | textura1 | Pendiente de revisión funcional y visual |
### shaders/imageprocessing/experimental (2)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [imageprocessing/experimental/minecarting.frag](../bin/data/shaders/imageprocessing/experimental/minecarting.frag) | Por definir | textura1 | Pendiente de revisión funcional y visual |
| [imageprocessing/experimental/stonetemple.frag](../bin/data/shaders/imageprocessing/experimental/stonetemple.frag) | Por definir | input_texture | Pendiente de revisión funcional y visual |
### shaders/imageprocessing/roto (1)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [imageprocessing/roto/smoothdisplace.frag](../bin/data/shaders/imageprocessing/roto/smoothdisplace.frag) | Por definir | input_texture | Pendiente de revisión funcional y visual |
### shaders/private (9)

| Archivo | Categoría tentativa | Entradas declaradas | Notas |
|---|---|---|---|
| [private/camdepth.frag](../bin/data/shaders/private/camdepth.frag) | Auxiliar / interno | camara, anterior, movimiento | Uso interno |
| [private/camdepth_motion.frag](../bin/data/shaders/private/camdepth_motion.frag) | Auxiliar / interno | camara, anterior | Uso interno |
| [private/camdepth_show.frag](../bin/data/shaders/private/camdepth_show.frag) | Auxiliar / interno | profundidad | Uso interno |
| [private/framedifference.frag](../bin/data/shaders/private/framedifference.frag) | Auxiliar / interno | textura1, textura2 | Uso interno |
| [private/mix.frag](../bin/data/shaders/private/mix.frag) | Auxiliar / interno | textura1, textura2 | Uso interno |
| [private/pointercloud.frag](../bin/data/shaders/private/pointercloud.frag) | Auxiliar / interno | — | Uso interno |
| [private/sequencer.frag](../bin/data/shaders/private/sequencer.frag) | Auxiliar / interno | slot0, slot1, slot2, slot3, slot4, slot5, slot6, slot7 | Uso interno |
| [private/transition_dither.frag](../bin/data/shaders/private/transition_dither.frag) | Auxiliar / interno | textura1, textura2 | Uso interno |
| [private/transition_warp.frag](../bin/data/shaders/private/transition_warp.frag) | Auxiliar / interno | textura1, textura2 | Uso interno |

## Registro de acuerdos

- **Tanda 1 — aceptación general de Nico:** «si esta buena esta tanda, sigamos». Se conservan sus ocho shaders en curado. Esta aceptación no certifica cada control ni resuelve las observaciones técnicas; `simplelines_2.0` continúa experimental.

- **2026-09-16 — Estructura acordada con Nico:** tres categorías principales, Generador, Efecto y Mezclador, con etiquetas combinables para familias visuales como color, geometría o glitch.
- «Por definir» es un estado provisional de clasificación, no una cuarta categoría final.
- Seguimos organizando en este documento y en el chat antes de trasladar la clasificación al programa.
- **2026-09-16 — Tanda 1 acordada con Nico:** nombres y clasificación de los ocho generadores de color y patrones; `simplelines_2.0` permanece experimental. Revisión visual pendiente.
- Pendientes: clasificación y nombres de las demás tandas, vocabulario final de etiquetas y selección de la colección.

## Probar el modo de curado

Desde la raíz del proyecto, ejecutar `./run-curated.sh` después de compilar con `make -j2`.
Abre IMPORT con los 18 shaders incorporados hasta ahora (12 generadores, 4 efectos y 2 mezcladores) y una sesión vacía.
El perfil de prueba se guarda en `dist/curated-profile`.

La lista editable es `release/shader-curated.json`: cada entrada tiene ruta, categoría,
nombre y descripción ES/EN, etiquetas y entradas de imagen. Para incorporar otra tanda,
agregar sus fichas y salir de IMPORT y volver a entrar (atajo `4` desde Nodos).
La lista se relee al entrar; no hace falta recompilar por cambios en el JSON.
Una lista vacía muestra cero shaders; una inválida también mantiene la biblioteca cerrada
y muestra un error, con el detalle en la terminal. Los favoritos se limitan a esa lista.
`simplelines_2.0` aparece explícitamente como experimental.

El ejecutable `bin/Guipper` sin el lanzador conserva el navegador habitual.
El modo filtra IMPORT; no bloquea la carga manual de archivos ni representa aprobación
para distribuir shaders. La selección oficial de publicación sigue siendo independiente.
