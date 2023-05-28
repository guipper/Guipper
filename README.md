<h1>Guipper - Software para Visuales en Tiempo Real</h1>

<h2>Breve descripción del proyecto y su propósito. <h2>

<p>Guipper es un software desarrollado para la realización de visuales en tiempo real utilizando GLSL fragment shaders. Permite ejecutar visuales parametrizados y generativos con un peso extremadamente bajo, además de brindar la posibilidad de agregar nuevos shaders.</p>

<h2>Características principales<h2>
Ejecución de visuales en tiempo real utilizando GLSL fragment shaders.
Visuales parametrizados y generativos.
Peso extremadamente bajo para un rendimiento óptimo.
Posibilidad de agregar nuevos shaders para ampliar la biblioteca.
Comunicación con otros programas mediante NDI y SPOUT.
Automatización de parámetros para crear animaciones fluidas.
Control OSC para establecer conexiones con interfaces externas.
Soporte para imágenes, videos y cámara web como fuentes visuales.
Randomización de parámetros para obtener resultados visuales sorprendentes.
Agradecimientos especiales a Kali Shade por contribuir con una amplia cantidad de shaders a la biblioteca oficial.
Diseño del software realizado por Lautaro Nuñez Muller.
Requisitos del sistema
Indica los requisitos necesarios para ejecutar Guipper en un entorno determinado. Asegúrate de mencionar las versiones compatibles de GLSL, así como cualquier otra dependencia o configuración adicional necesaria.

<h2>Instalación<h2>
Proporciona instrucciones paso a paso sobre cómo instalar y configurar Guipper. Incluye información sobre cómo descargar y compilar el código fuente, así como cualquier configuración adicional requerida.

<h2>Guía de uso<h2>
Ofrece una guía detallada sobre cómo utilizar Guipper de manera efectiva. Incluye ejemplos de código, explicaciones de las funciones clave y capturas de pantalla o videos para mostrar los resultados visuales que se pueden lograr.

<h2>Contribuciones<h2>
Si deseas invitar a otros desarrolladores a contribuir al proyecto Guipper, describe cómo pueden hacerlo. Proporciona información sobre cómo clonar el repositorio, configurar el entorno de desarrollo y enviar solicitudes de extracción.

<h2>Licencia<h2>
Este programa se distribuye bajo la Licencia MIT. Asegúrate de incluir una copia de la licencia en tu repositorio.

<h2>Contacto<h2>
Proporciona información de contacto para consultas o soporte adicional. Puedes incluir tu dirección de correo electrónico, enlaces a perfiles en redes sociales o un canal de comunicación específico para el proyecto.

<h2>Roadmap<h2>

IDE interno para livecoding.
Sistema de patches (tema de selección de parámetros y pestañas).
Sistema de carga de shaders interno (además del arrastrar y soltar).
Servidor para subir y descargar shaders hechos por la comunidad.
Adaptación de shadertoys.
Revisión y limpieza de shaders existentes y agregar nuevos.
Finalizar la versión multiplataforma para LINUX, MAC y WINDOWS.
Agregar soporte Syphon a la versión de MAC.
Agregar soporte audiorítmico a los parámetros mediante FFT.
Agregar sistema de MIDI para controlar los parámetros.
Implementar un sistema de uniforms para vec2, vec3 y vec4.
Limpiar las funciones que están en el archivo .common.
Implementar un sistema de branches o versionado del mismo shader dentro de la interfaz (requiere tener el IDE interno funcionando).
Interfaz de triggers similar a la de Resolume, con una grilla.
