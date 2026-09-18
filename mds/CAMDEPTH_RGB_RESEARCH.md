# CAMDEPTH RGB: investigación y plan de prototipo

Fecha: 2026-09-18. Estado: **pausado por decisión del usuario**. Prototipos CPU y CUDA ejecutados con ambos modelos; integración, webcam y carga simultánea pendientes. No continuar hasta que el usuario retome el tema.

## Objetivo

Usar una webcam RGB común como fuente de profundidad relativa para visuales. El usuario no necesita Kinect. El módulo futuro también debería aceptar video o una textura del grafo. No presentar el resultado como distancia medida en metros.

## Base actual de Guipper

`src/JPbox/jp_box_camdepth.cpp` comparte la captura con CAMARITA. Sus shaders combinan detalle local, brillo, posición vertical, diferencias entre frames y otras pistas visuales. Ya tiene suavizado, control de bordes, rango, inversión y espejo. Añadir controles similares no soluciona las ambigüedades del estimador.

Conservar este recorrido como modo clásico y mantener la interpretación de composiciones existentes. Kinect y POINTERCLOUD usan profundidad métrica por un recorrido separado; no sustituir sus datos por profundidad RGB relativa.

## Comparación y selección

| Proyecto | Papel en el experimento | Decisión |
|---|---|---|
| Depth Anything V2 Small | Referencia monocular por imagen, 24,8 M de parámetros | Candidato A |
| Video Depth Anything Small | Modelo temporal, 28,4 M; variante streaming experimental | Candidato B |
| MiDaS | Referencia alternativa con modelos pequeños y experiencia de despliegue | Reserva; evitar ampliar la primera comparación |
| depth-anything.cpp | Implementación nativa de modelos Depth Anything, API C y backend Vulkan | Evaluar después de establecer una referencia de calidad |

Los repositorios oficiales y las fichas consultadas identifican los pesos **Small** de A y B como Apache-2.0. No extender esa conclusión a Base/Large, cuyo README indica CC-BY-NC-4.0. Conservar licencia y atribuciones al distribuir. El port C++ publica código MIT y mantiene la licencia original de cada peso convertido.

Fuentes primarias:

- [Depth Anything V2](https://github.com/DepthAnything/Depth-Anything-V2), [modelo Small](https://huggingface.co/depth-anything/Depth-Anything-V2-Small).
- [Video Depth Anything](https://github.com/DepthAnything/Video-Depth-Anything), [modelo Small](https://huggingface.co/depth-anything/Video-Depth-Anything-Small).
- [MiDaS](https://github.com/isl-org/MiDaS).
- [depth-anything.cpp](https://github.com/localai-org/depth-anything.cpp).

La elección A/B es una propuesta para Guipper, no un resultado de rendimiento local. Que un modelo sea pequeño no implica que alcance 30 FPS junto con los shaders del show.

## Fuentes reproducibles

| Fuente | Revisión consultada |
|---|---|
| Depth Anything V2 | `a561b849ebae10a6f5ef49e26c83cbbcd36c71bf` |
| Video Depth Anything | `4f5ae23172ba60fd7bc11ef671cca678842c7072` |
| depth-anything.cpp | `14f7461d1f704761a038ac9f50dbde8fdb7275e2` |
| Pesos DA2 Small (Hugging Face) | `03876f8651c73a60fe4c2c48294e09fcb6838fcf` |
| Pesos VDA Small (Hugging Face) | `256875362cff76724b920335dfb4b29dd611f66e` |

Las copias de los dos repositorios oficiales están en `dist/depth-research/`, fuera del código de producto. También se guardaron README, licencias, requisitos y metadatos de los modelos. Los pesos se descargaron en la etapa de prototipo descrita más abajo.

## Cómo construir la prueba independiente

1. Crear entornos Python separados: los requisitos de VDA fijan versiones antiguas de torch, torchvision, numpy y xformers. No instalar esos requisitos en el Python del sistema ni asumir que soportan la GPU AMD disponible.
2. Ejecutar primero inferencia CPU de referencia sobre un clip corto con los pesos Small oficiales. Registrar versiones efectivas, revisión de fuentes y SHA-256 del archivo de pesos.
3. Para A, adaptar `DepthAnythingV2.infer_image` del repositorio oficial. Para B, usar `VideoDepthAnything.infer_video_depth_one` de `video_depth_stream.py`, conservando el estado temporal entre frames. Reiniciarlo entre clips/cámaras y ante discontinuidades.
4. Respetar el orden de canales: el ejemplo de A recibe BGR de OpenCV; el ejemplo streaming de B convierte a RGB. Mantener el preprocesamiento oficial de cada modelo.
5. Ambos reciben los mismos frames, mismo campo de visión y misma resolución solicitada. Registrar el tamaño interno efectivo y cualquier padding/redimensionado. Probar 518 como referencia y una entrada menor válida como experimento de velocidad.
6. Guardar mapas float sin normalización por frame y muestras de preview. Para comparar visualmente, usar un rango fijo por clip o estimación robusta estabilizada. El min/max independiente en cada frame puede introducir parpadeo artificial.
7. Separar latencia de carga, primer resultado, preprocesamiento, inferencia sincronizada con GPU y postprocesamiento. Calentar cada modelo antes de medir p50/p95 sostenidos; no convertir FPS del archivo en FPS de inferencia.

El script streaming oficial de B acumula salidas en memoria y exporta al terminar. Sirve para estudiar el modelo, pero no es una arquitectura de webcam lista para incorporar en Guipper.

## Hardware y ejecución

La sesión anterior identificó una Radeon 760M en Linux. La inspección actual del Python por defecto no encontró torch, numpy, OpenCV ni ONNX Runtime; esto no descarta otros entornos instalados.

- CPU: referencia funcional antes de optimizar, sin promesa de fluidez.
- Windows: estudiar ONNX Runtime/Windows ML y sus proveedores, verificando la exportación del modelo antes de elegirlos.
- Linux AMD: investigar Vulkan mediante un runtime nativo. No asumir soporte ROCm de esta iGPU.
- El port C++ es prometedor para integración; validar paridad con la referencia usando exactamente el mismo modelo y preprocesamiento. Sus benchmarks publicados corresponden a otro hardware y no prueban latencia en esta máquina.

Fuentes: [ONNX Runtime, proveedores](https://onnxruntime.ai/docs/execution-providers/), [Windows ML](https://onnxruntime.ai/docs/get-started/with-windows.html), [backend Vulkan de ncnn](https://github.com/Tencent/ncnn). ncnn es un runtime, no un modelo de profundidad ni una conversión DA2 ya verificada aquí.

## Clips y criterio de elección

Preparar clips locales reproducibles de 10–20 segundos, sin subir cámara a servicios:

- Escena quieta: evaluar oscilación temporal en zonas estables.
- Persona, manos y pelo: bordes y retraso al moverse.
- Objeto negro y blanco a igual distancia: separación de geometría e iluminación.
- Persona entrando/saliendo: cambios de escala global del estimador.
- Poca luz y luz cambiante.
- Paneo de cámara, oclusión y objeto que aparece de golpe.

Comparar el modo clásico, A y B. Guardar resolución, dispositivo, modelo, precisión, carga, primer frame, p50/p95, memoria y observaciones. Sin ground truth no llamar «precisión métrica» a la evaluación visual. La variación entre frames con movimiento tampoco mide por sí sola parpadeo.

Objetivo provisional para una primera versión: explorar al menos 15 actualizaciones de profundidad por segundo y p95 de procesamiento menor a 100 ms a resolución reducida, midiendo aparte edad del frame y retraso visible. Son metas de aceptación a ajustar con el usuario, no resultados alcanzados. Repetir con una composición representativa en Guipper antes de elegir el backend final.

## Integración posterior, si el prototipo resulta útil

Captura compartida → frame con timestamp → worker de inferencia → mapa float relativo → postproceso GPU → textura de salida.

- Cola de tamaño uno para nuevas capturas: reemplazar pendientes por el frame reciente; no acumular segundos de retraso.
- Mantener GL/FBO en el hilo con contexto; el worker publica resultados inmutables.
- Mostrar FPS de inferencia, edad del resultado y backend realmente usado.
- Guardar controles/modelo elegido en la composición, no mapas temporales ni historia neuronal.
- Escala normalizada estable, rango y máscara suave; suavizado temporal con reinicio ante corte o cambio de fuente.
- Mantener profundidad float durante el procesamiento; convertir a grises/color sólo para visualización.
- Conservar modo clásico; fallos de modelo/cámara deben tener estado explícito.
- Preparar la fuente para video y textura además de webcam, evitando copiar/descargar a CPU frames que no se procesarán.

## Entrega de esta investigación

Hecho: lectura del código existente, comparación inicial, selección A/B, revisión de licencias declaradas y revisiones fijadas, copias de fuentes oficiales y protocolo de medición.

Al cerrar la investigación inicial faltaban entornos, pesos e inferencia. La sección de resultados posterior registra lo ejecutado; siguen pendientes medición GPU, clips representativos y elección de runtime. CAMDEPTH permanece sin cambios.

## Prototipo CPU ejecutable

Se añadió `tools/depth_benchmark.py`. A y B se ejecutan en procesos separados con el mismo entorno CPU aislado y versiones idénticas; no se instalaron los requirements completos de las demos. xformers es opcional en estos recorridos y no se instala para esta prueba CPU.

Entorno probado: Python 3.12, torch 2.6.0+cpu, torchvision 0.21.0+cpu, numpy 1.26.4, OpenCV headless 4.11.0, einops 0.8.1 y easydict 1.13. Las versiones transitivas quedaron en `dist/depth-research/environment-cpu.txt`.

Preparación para repetir en Linux, partiendo de las fuentes fijadas arriba:

```bash
python3 -m venv dist/depth-research/venv
dist/depth-research/venv/bin/pip install torch==2.6.0 torchvision==0.21.0 --index-url https://download.pytorch.org/whl/cpu
dist/depth-research/venv/bin/pip install -r tools/depth_benchmark_requirements.txt
```

En esta máquina faltaba ensurepip. Se inicializó pip con el bootstrap oficial de PyPA **dentro del entorno virtual ya creado**, sin instalar paquetes del sistema.

Los pesos se descargaron desde las revisiones de Hugging Face fijadas arriba a `dist/depth-research/models/`. Archivos y SHA-256:

- `depth_anything_v2_vits.pth`: `715fade13be8f229f8a70cc02066f656f2423a59effd0579197bbf57860e1378`.
- `video_depth_anything_vits.pth`: `13379300b739e659f076a59d52e9801bd8d38c541a7e71f73bbca4dcfb013609`.

Ejecución (cada salida debe ser un directorio nuevo; no ejecutar los modelos simultáneamente al medir CPU):

```bash
dist/depth-research/venv/bin/python tools/depth_benchmark.py --model da2 --out dist/depth-research/results/da2-repeat
dist/depth-research/venv/bin/python tools/depth_benchmark.py --model vda --out dist/depth-research/results/vda-repeat
```

Opciones: `--video archivo.mp4`, `--size 518`, `--frames 64`, `--threads 6`, `--static`. El modo static repite el primer frame para detectar variación interna; no sustituye un clip real de una cámara quieta con ruido y cambios de exposición.

Cada ejecución genera un JSON con revisiones, hashes, tiempos por frame, p50/p95, tamaño de entrada efectivo y versiones; mapas float en `depth.npz`, tres capturas y `preview.avi`. La preview se reproduce a la velocidad del clip original: **no es una demostración de inferencia en tiempo real**. Conserva un rango visual fijo por ejecución (percentiles 2–98 del conjunto); esos valores pueden ser distintos entre modelos porque su escala relativa es arbitraria.

Los tiempos abarcan el preprocesamiento del modelo, su inferencia CPU y el reescalado de salida. Excluyen decodificación de video, redimensionado previo a 640 px, escritura y visualización; no equivalen a latencia cámara-pantalla. Se descartan cinco frames iniciales para los percentiles; el primero se registra por separado. No se añade suavizado externo.

## Resultados iniciales medidos

CPU AMD Ryzen 5 7640HS, 6 hilos, float32. Ejecuciones secuenciales; no se midió competencia con Guipper. Mismo clip `davis_rollercoaster.mp4` del repositorio VDA, reducido a 640 × 360 antes del modelo. No es una prueba de webcam en vivo.

| Modelo | Entrada real | Frames / calentamiento | p50 ms | p95 ms | FPS de procesamiento |
|---|---|---|---|---|---|
| da2-252 | 448 × 252 | 64 / 5 | 162.3 | 191.7 | 6.06 |
| vda-252 | 448 × 252 | 64 / 5 | 330.0 | 379.9 | 2.97 |
| da2-518 | 924 × 518 | 24 / 5 | 1696.0 | 1848.7 | 0.58 |
| vda-518 | 924 × 518 | 24 / 5 | 2523.3 | 2633.1 | 0.40 |

Las muestras a 518 son sólo 24 frames, de los cuales se miden 19 tras calentamiento; sirven para detectar costo, no para certificar rendimiento sostenido. Las pruebas a 252 recorren 64 frames e incluyen la evolución inicial del caché temporal de VDA.

Diagnóstico de imagen repetida (24 frames, cinco de calentamiento): diferencia absoluta media entre mapas consecutivos, dividida por el rango visual fijo de la ejecución:

- da2-static: 0.00000000.
- vda-static: 0.00079121.

Esta diferencia mide variación ante una entrada idéntica, no estabilidad de una escena real ni precisión. No adjudicar una mejora temporal en video a partir de este diagnóstico. En las muestras visuales ambos modelos separan estructuras oscuras del cielo; falta evaluar personas, manos, pelo, interiores y baja luz.

Decisión provisional: continuar con DA2 Small como primera ruta de rendimiento y mantener VDA Small como comparador temporal. Ninguno alcanza la meta provisional de 15 FPS en esta referencia CPU. Investigar aceleración GPU antes de integrar inferencia al producto. No reducir simplemente la frecuencia y anunciar 60 FPS de profundidad porque la interfaz dibuje a esa velocidad.

Artefactos locales: `dist/depth-research/results/{da2-252,vda-252,da2-518,vda-518,da2-static,vda-static}/`. Los videos son previews offline a FPS de origen; JSON y mapas float son la evidencia de la medición. Los pesos, repositorios y artefactos grandes permanecen fuera del control de versiones de Guipper.

## Aceleración GPU: resultado local

La enumeración Vulkan detectó dos GPU físicas: AMD Radeon 760M y NVIDIA GeForce RTX 4050 Laptop GPU. La referencia OpenGL anterior sólo había identificado la integrada. `nvidia-smi` confirmó 6141 MiB de VRAM y driver 580.178.04.

Se creó un segundo entorno `dist/depth-research/venv-cuda`, con torch 2.6.0+cu124 y torchvision 0.21.0+cu124. El entorno CPU se conserva. Ambas pruebas usan float32, sin xformers y con TF32 desactivado. El benchmark admite `--device cuda`, verifica disponibilidad y rechaza volver silenciosamente a CPU. Sincroniza CUDA antes y después de medir cada frame. No había un proceso Guipper activo durante estas mediciones.

| Modelo | Entrada real | FPS CPU | FPS CUDA | p95 CUDA ms | Pico tensor CUDA MiB |
|---|---|---:|---:|---:|---:|
| da2 Small | 448 × 252 | 6.06 | 48.92 | 23.68 | 155.0 |
| da2 Small | 924 × 518 | 0.58 | 8.73 | 117.71 | 413.9 |
| vda Small | 448 × 252 | 2.97 | 39.39 | 26.26 | 407.7 |
| vda Small | 924 × 518 | 0.40 | 6.35 | 159.72 | 1127.2 |

El pico de memoria es el del allocator de tensores PyTorch, no toda la VRAM del proceso ni del sistema. Los FPS son throughput del procesamiento con transferencia de entrada/salida, no FPS de webcam ni de Guipper. Se mantienen los límites de muestra anteriores: 64 frames a 252, 24 a 518 y cinco descartados para calentamiento. No es una prueba sostenida ni bajo carga de un show.

### Paridad CPU/CUDA

Se verificaron antes de comparar: mismo modelo, revisión, hash de pesos, hash del video, número de frames, dimensiones internas y precisión. En los cuatro casos, la correlación de los mapas supera 0,999999999999 y el error absoluto medio dividido por el rango p2–p98 CPU es menor a 1,2e-7. Esto confirma coincidencia numérica práctica en este clip, no precisión frente a profundidad real.

Evidencia: `dist/depth-research/results/cpu-cuda-parity.json`, `*-cuda-*/report.json`, mapas float y previews en los respectivos directorios. Dependencias efectivas: `dist/depth-research/environment-cuda.txt`.

### Repetir CUDA

```bash
python3 -m venv dist/depth-research/venv-cuda
dist/depth-research/venv-cuda/bin/pip install torch==2.6.0 torchvision==0.21.0 --index-url https://download.pytorch.org/whl/cu124
dist/depth-research/venv-cuda/bin/pip install -r tools/depth_benchmark_requirements.txt
dist/depth-research/venv-cuda/bin/python tools/depth_benchmark.py --model da2 --device cuda --out dist/depth-research/results/da2-cuda-repeat
```

### Ruta AMD/Vulkan

Se descargó `depth-anything.cpp` en la revisión fijada y su submódulo ggml `eced84c86f8b012c752c016f7fe789adea168e1e`. La configuración de CMake con `DA_GGML_VULKAN=ON` encontró Vulkan 1.3.275, pero no el compilador `glslc`; no llegó a compilar ni ejecutar. No se instalaron paquetes del sistema para resolverlo en esta etapa. La prueba con CUDA oficial se priorizó al descubrir la RTX, porque permite comparar sin convertir los modelos.

No declarar comprobado el port C++, Vulkan en AMD ni Windows. Quedan como rutas posteriores de despliegue/compatibilidad.

### Decisión tras GPU

Ambos candidatos superan 30 FPS de procesamiento en la entrada reducida del clip de prueba. DA2 deja más margen; VDA sigue siendo candidato viable para comparar estabilidad temporal. A 518 ninguno alcanza 15 FPS en float32. La primera prueba en vivo debería usar entrada reducida y limitar la frecuencia de inferencia, conservando render independiente y cola del frame más reciente.

Siguiente etapa: visor independiente con webcam, selección explícita de dispositivo y backend, edad del resultado, comprobación de cola acotada, cambio de fuente y desconexión. Comparar personas/manos/pelo y luego medir con Guipper renderizando. Sólo después elegir el modelo por defecto e integrar CAMDEPTH. Optimizar precisión o exportar a un runtime nativo requiere una nueva comprobación de paridad.
