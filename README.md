#  Software para Visuales en Tiempo Real

## Breve descripción del proyecto y su propósito.

Guipper es un software desarrollado para la realización de visuales en tiempo real utilizando GLSL fragment shaders. Permite ejecutar visuales parametrizados y generativos con un peso extremadamente bajo, además de brindar la posibilidad de agregar nuevos shaders.

Agradecimientos especiales a Kali Shade por contribuir con una amplia cantidad de shaders a la biblioteca oficial.
Diseño del software realizado por Lautaro Nuñez Muller.

## Características principales
<ul>
  <li>Ejecución de visuales en tiempo real utilizando GLSL fragment shaders.</li>
  <li>Visuales parametrizados y generativos.</li>
  <li>Peso extremadamente bajo para un rendimiento óptimo.</li>
  <li>Posibilidad de agregar nuevos shaders para ampliar la biblioteca.</li>
  <li>Comunicación con otros programas mediante NDI y SPOUT.</li>
  <li>Automatización de parámetros para crear animaciones fluidas.</li>
  <li>Control OSC para establecer conexiones con interfaces externas.</li>
  <li>Soporte para imágenes, videos y cámara web como fuentes visuales.</li>
  <li>Randomización de parámetros para obtener resultados visuales sorprendentes.</li>
  
</ul>

## Instalación
Proporciona instrucciones paso a paso sobre cómo instalar y configurar Guipper. Incluye información sobre cómo descargar y compilar el código fuente, así como cualquier configuración adicional requerida.

## Guía de uso
Ofrece una guía detallada sobre cómo utilizar Guipper de manera efectiva. Incluye ejemplos de código, explicaciones de las funciones clave y capturas de pantalla o videos para mostrar los resultados visuales que se pueden lograr.

## Contribuciones
Si deseas invitar a otros desarrolladores a contribuir al proyecto Guipper, describe cómo pueden hacerlo. Proporciona información sobre cómo clonar el repositorio, configurar el entorno de desarrollo y enviar solicitudes de extracción.

## Licencia
Este programa se distribuye bajo la Licencia MIT. Asegúrate de incluir una copia de la licencia en tu repositorio.

## Contacto
Proporciona información de contacto para consultas o soporte adicional. Puedes incluir tu dirección de correo electrónico, enlaces a perfiles en redes sociales o un canal de comunicación específico para el proyecto.

## Roadmap
<ul>
  <li>IDE interno para livecoding.</li>
  <li>Sistema de patches (tema de selección de parámetros y pestañas).</li>
  <li>Sistema de carga de shaders interno (además del arrastrar y soltar).</li>
  <li>Servidor para subir y descargar shaders hechos por la comunidad.</li>
  <li>Adaptación de shadertoys.</li>
  <li>Revisión y limpieza de shaders existentes y agregar nuevos.</li>
  <li>Finalizar la versión multiplataforma para LINUX, MAC y WINDOWS.</li>
  <li>Agregar soporte Syphon a la versión de MAC.</li>
  <li>Agregar soporte audiorítmico a los parámetros mediante FFT.</li>
  <li>Agregar sistema de MIDI para controlar los parámetros.</li>
  <li>Implementar un sistema de uniforms para vec2, vec3 y vec4.</li>
  <li>Limpiar las funciones que están en el archivo .common.</li>
  <li>Implementar un sistema de branches o versionado del mismo shader dentro de la interfaz (requiere tener el IDE interno funcionando).</li>
  <li>Interfaz de triggers similar a la de Resolume, con una grilla.</li>
</ul>
