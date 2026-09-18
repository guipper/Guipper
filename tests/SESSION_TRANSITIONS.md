# Transiciones de composición

Primera etapa: captura RGBA de la salida completa saliente (incluye FINAL),
fundida con la nueva composición viva. La duración se toma del ajuste existente
al cargar; el efecto de sesión en esta etapa es un fundido. Los cambios de nodo
conservan sus propios efectos.

El reloj comienza después del primer render entrante. Una carga fallida antes
de instalar el grafo conserva la salida anterior. Si llega otro cambio durante
el fundido, se captura la mezcla visible. Clear cancela y libera la transición.
La carga de una composición vacía termina en transparencia.

## Prueba nativa

Después de compilar:

```sh
xvfb-run -a python3 tests/run_session_fade.py
```

En Windows, con escritorio y el ejecutable compilado:

```bat
python tests/run_session_fade.py --binary bin/Guipper.exe
```

El runner copia ejecutable y datos a un directorio temporal y usa un perfil
separado. Comprueba los píxeles de FINAL saliente/entrante, primer frame,
punto intermedio, extremo final, carga fallida, interrupción, salida por textura,
recorte y composición vacía. El log queda en `dist/session-fade.log`.

## Límites de esta etapa

- La saliente queda congelada durante el fundido.
- Lectura y construcción del grafo siguen siendo sincrónicas; una composición
  pesada todavía puede detener la UI durante la carga.
- Primer render no implica que un decoder asíncrono de imagen/video ya haya
  entregado todos sus recursos. La disponibilidad por tipo de media y la
  preparación escalonada quedan para la próxima etapa.
- Las salidas vinculadas directamente a un nodo conservan esa selección;
  el fundido de sesión corresponde a las que siguen la salida principal.
- Windows/Spout requiere verificación allí; Linux no valida esa integración.

Revisión manual adicional: cámaras y videos reales, CUE activo, mapping de varias
pantallas, transparencia sobre fondos claros/oscuros y sesiones pesadas.
