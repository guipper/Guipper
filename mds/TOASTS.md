# Avisos reutilizables

`src/JPutils/jp_toast.h` contiene el estado puro, sin OpenGL ni `ofApp`.
`src/JPgui/jp_toast_view.*` resuelve layout, dibujo y captura del gesto del mouse.
La aplicación traduce los mensajes y ejecuta las acciones; el gestor solo entrega
`ToastEvent` mediante `takeEvents()`.

## Publicación

```cpp
jp::Toast toast;
toast.id = "export"; // Identificador estable: repetir actualiza y reinicia el tiempo.
toast.state = jp::ToastState::Success;
toast.message = language == 0 ? "Export complete." : "Exportación terminada.";
toasts.publish(std::move(toast));
```

Duración negativa selecciona el valor predeterminado por estado (4/6/8/10 s);
cero mantiene el aviso hasta cerrarlo. Se permiten hasta dos acciones `{id, label}`.
`dismissAction` convierte el cierre en un evento para la aplicación: se usa para
que × y Descartar recuperación compartan el mismo manejador. `close(id)` inicia
la salida de 160 ms. Los avisos persistentes no se expulsan al llegar al límite.
Si los tres fueran persistentes, `publish` rechaza un cuarto aviso y devuelve false.

El hover pausa cada aviso; cualquier modal bloqueante pausa todos los plazos y
deshabilita acciones. La vista captura desde la pulsación hasta la liberación,
incluso si el cursor abandona el aviso. No se asigna un atajo a X.

## Integraciones

- Recuperación: persistente, F9/Recuperar y F8/Descartar/×. Un backup o una carga
  fallida conservan la recuperación y publican un error independiente.
- `saveSession(path, true)`: confirmación de guardado manual, solo tras éxito.
  El valor predeterminado es silencioso para backups y guardados internos.
- `saveSettings()` devuelve el resultado real; solo el botón manual publica aviso.
- Actualización disponible: acción para abrir el mismo panel que F10.
- Vista compacta de una sola línea, tipografía reducida y fondo translúcido.
  Los mensajes largos se abrevian con puntos suspensivos sin cortar caracteres UTF-8;
  las acciones y el cierre permanecen en la misma fila.

Futuras integraciones: exportación terminada, errores de importación, compilación
de shaders y conexión MIDI. No se publican avisos de ajustes automáticos de preview.

## Comprobaciones

- `make -C tests run`: incluye el gestor puro (`toast_tests`).
- `GUIPPER_PERSISTENCE_TEST=toasts`: prueba nativa de guardado, fallos de escritura,
  recuperación por atajos y eventos, clic de cierre, modales y captura sobre NODES
  e IMPORT. Ejecutar en una copia temporal del ejecutable y `bin/data`, con
  `GUIPPER_USER_ROOT` temporal; el harness genera y reemplaza fixtures de composición.
- `GUIPPER_TOAST_CAPTURE=/ruta/absoluta`: junto al modo anterior genera cuatro
  capturas ES/EN de 1440×840 y 400×430, con tres avisos y nombres largos.
- Linux descubre los nuevos `.cpp` bajo `src`; Visual Studio tiene entradas
  explícitas en el proyecto y sus filtros.
