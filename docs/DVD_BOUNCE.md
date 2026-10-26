# DVD BOUNCE

Crear con **Shift+B** y conectar una caja a **imagen**. La entrada se reduce,
se desplaza y rebota en los cuatro bordes como el salvapantallas DVD.
La salida conserva el tamaño de render de Guipper y deja transparente el resto.
Una imagen con fondo opaco mueve también ese fondo; para mover solo un logo,
usar una entrada con transparencia.

- **size**: fracción del tamaño máximo que cabe sin deformar la imagen (0,02–1).
- **speed x / speed y**: longitudes del lienzo por segundo (−1–1). Cero
  detiene ese eje; cambiar el signo invierte el movimiento.
- **pause**: congela la posición mientras la imagen de entrada sigue actualizándose.

Los controles admiten la automatización habitual, incluido seguimiento.
Guardar, duplicar o deshacer conserva los parámetros; la fase de movimiento
es temporal y comienza de nuevo al reconstruir la caja. Desactivar o dejar
la caja sin render evita acumular tiempo de movimiento. Bypass conserva la
entrada sin transformación.

La prueba `python3 tests/dvd_graphics_test.py` usa un perfil temporal y verifica
entrada, proporciones, rebotes, pausa, cambio a render vertical y persistencia.
