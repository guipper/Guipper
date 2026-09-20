# Pestañas de grupos

Compilar con `make Release -j2`. Ejecutar con una copia aislada del binario y
`bin/data`, desde el directorio del binario:

```sh
GUIPPER_UISHOT=groups GUIPPER_USER_ROOT=/tmp/guipper-group-profile ./Guipper
```

El modo genera cinco capturas en `data/uishots/groups`: barra ancha con un grupo seleccionado fuera de la primera página, ventana
estrecha con inspector abierto, selección dentro de un grupo, grupo anidado y
grupos desplazados.
Comprueba las zonas clickeables de cada pestaña visible, límites de ventana,
visibilidad automática de la selección, barra y flechas fuera del inspector,
restauración del zoom de MAIN y que la rueda y seleccionar la pestaña activa
no cambien el zoom. También comprueba la activación del renombrado con doble
clic y su cancelación con Escape. Termina con error si falla una comprobación.

Comprobación manual adicional:

- Recorrer grupos con flechas y rueda; MAIN permanece accesible. Seleccionar
  una caja cuya pestaña esté fuera de la página: debe aparecer resaltada.
  Volver a desplazar la barra y pulsar la misma caja: debe mostrarla nuevamente.
- Abrir/cerrar el inspector y redimensionar: pestañas y flechas deben quedar
  completamente a su izquierda cuando ambos ocupan la misma altura.
- Entrar en un grupo, mover/ajustar el lienzo y volver: conserva su encuadre.
- Hacer doble clic en la pestaña del grupo actual, editar el nombre, confirmar
  con Enter o cancelar con Escape. Probar nombres con acentos y nombres largos.
- Pulsar y arrastrar desde la barra sobre nodos: no debe moverlos ni conectarlos.
- Guardar y volver a abrir una composición con grupos anidados.

Validación local (2026-09-20): compilación Release Linux, 18 suites del núcleo
y los cinco casos gráficos bajo Xvfb aprobados. Capturas revisadas a 1280×720 y
640×720. No se realizó una compilación Windows o macOS en esta modificación.

El modo también captura búsqueda con un resultado, menú contextual y búsqueda
sin resultados. Verifica localización por UID, renombrado desde el menú,
Escape, rechazo de UID inexistentes y memoria de página al regresar de un grupo.
La lupa abre el buscador; clic derecho sobre una pestaña abre sus acciones.
