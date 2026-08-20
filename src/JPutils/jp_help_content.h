#pragma once

#include "ofMain.h"
#include <vector>

// The HELP screen's content, as data.
//
// It used to be two parallel `string es[64]` / `en[64]` arrays inside
// draw_instrucciones(), and the FORMATTING was decided by substring-matching
// the text itself - a line was a heading because it contained "KEYS:". That is
// why "NAVIGATION" rendered as plain body text while "KEYS:" rendered cyan, and
// why the keys were baked into the prose ("z : Toggle cue") instead of getting
// their own column.
//
// Here a row is a row of data. Adding a line is one E(...) entry; there is no
// formatting rule to touch and nothing is ever inferred from the wording.
//
// Spanish is deliberately written WITHOUT accents, matching the original arrays
// - ofTrueTypeFont is loaded with the default (Latin) charset and accented
// glyphs came out blank.
namespace jp_help
{
	enum class Kind
	{
		Heading,   // section title + rule
		Step,      // numbered quick-start row
		Entry,     // keys gutter + description
		Note,      // prose spanning the full content width, no keys
		Gap        // vertical space
	};

	// Which screen a shortcut is actually live on. Most of the letter keys sit
	// inside `if (pantallaActiva == NODOS)` in ofApp::keyPressed, but the old
	// help listed them as though they worked everywhere.
	enum class Scope
	{
		Global,
		Nodes,
		// Live only while the paint editor panel is open. Its own scope so the
		// panel's shortcuts modal can filter the table down to exactly these.
		Paint,
		Import,
		Editor,
		Midi,
		Settings
	};

	struct Line
	{
		Kind kind = Kind::Entry;
		Scope scope = Scope::Global;
		const char *keys = "";   // left gutter; empty for Heading/Note/Gap
		const char *en = "";
		const char *es = "";
	};

	inline Line H(const char *en, const char *es)
	{
		return {Kind::Heading, Scope::Global, "", en, es};
	}
	inline Line N(const char *en, const char *es, Scope s = Scope::Global)
	{
		return {Kind::Note, s, "", en, es};
	}
	inline Line S(const char *number, const char *en, const char *es)
	{
		return {Kind::Step, Scope::Global, number, en, es};
	}
	inline Line GAP()
	{
		return {Kind::Gap, Scope::Global, "", "", ""};
	}
	inline Line E(const char *keys, const char *en, const char *es,
		Scope s = Scope::Global)
	{
		return {Kind::Entry, s, keys, en, es};
	}

	// Short tag drawn at the right of a row. Empty for Global, so only the
	// screen-scoped rows carry one.
	inline const char *scopeTag(Scope s)
	{
		switch (s)
		{
		case Scope::Nodes:    return "NODES";
		case Scope::Paint:    return "PAINT";
		case Scope::Import:   return "IMPORT";
		case Scope::Editor:   return "EDITOR";
		case Scope::Midi:     return "MIDI";
		case Scope::Settings: return "SETTINGS";
		default:              return "";
		}
	}

	// language 0 == ENGLISH, matching draw_shaderindex and the frame title.
	// The old body array had this inverted, so the app booted with a Spanish
	// body under an English "HELP" header.
	inline const char *text(const Line &l, int language)
	{
		return language == 0 ? l.en : l.es;
	}

	inline const std::vector<Line> &table()
	{
		static const std::vector<Line> t = {

		// ------------------------------------------------------------------
		H("QUICK START", "INICIO RAPIDO"),
		S("1", "Add a source: drag in an image or video, or add Camera/Kinect from NODES.",
			"Agrega una fuente: arrastra una imagen o video, o agrega Camara/Kinect desde NODES."),
		S("2", "Open IMPORT (4), find an effect, and load it as a box.",
			"Abre IMPORT (4), busca un efecto y cargalo como caja."),
		S("3", "Drag from the source outlet to the effect inlet to connect them.",
			"Arrastra desde la salida de la fuente hasta la entrada del efecto para conectarlos."),
		S("4", "Select a box to edit its inspector. Double-click a box to make it the active render.",
			"Selecciona una caja para editar su inspector. Haz doble click para convertirla en el render activo."),
		S("5", "Set outputs and BPM in SETTINGS, map live controls in MIDI, and use CUE to prepare changes off-air.",
			"Configura salidas y BPM en SETTINGS, asigna controles en MIDI y usa CUE para preparar cambios fuera del aire."),
		GAP(),

		// ------------------------------------------------------------------
		H("GETTING AROUND", "MOVERSE POR EL PROGRAMA"),
		E("1 - 6",
			"Switch screen: 1 NODES, 2 SETTINGS, 3 HELP, 4 IMPORT, 5 EDITOR, 6 MIDI",
			"Cambiar de pantalla: 1 NODES, 2 SETTINGS, 3 HELP, 4 IMPORT, 5 EDITOR, 6 MIDI"),
		N("The top bar has the same six screens as buttons, then a separate group: CUE and MAP.",
			"La barra superior tiene las mismas seis pantallas como botones, y aparte un grupo: CUE y MAP."),
		E("CUE",
			"Opens the cue preview panel. Lights up when a box is cued",
			"Abre el panel de preview del cue. Se enciende cuando hay una caja en cue"),
		E("MAP",
			"Projection mapping editor. Greyed out until a mapping shader is selected",
			"Editor de mapeo. Deshabilitado hasta que selecciones un shader de mapeo"),
		E("ESC",
			"Closes the topmost open thing, one layer per press: field, dropdown, panel, modal. Never changes screen",
			"Cierra lo que este mas arriba, una capa por vez: campo, lista, panel, modal. Nunca cambia de pantalla"),
		E("Enter",
			"Applies the focused text field. ESC discards it",
			"Aplica el campo de texto enfocado. ESC lo descarta"),
		E("Mouse wheel",
			"Scrolls the list under the pointer: this screen, IMPORT, MIDI, SETTINGS",
			"Scrollea la lista debajo del puntero: esta pantalla, IMPORT, MIDI, SETTINGS"),
		N("Drag files onto the window to load them. Several at once are laid out in a grid where you dropped them. Takes .frag, .xml, images and videos.",
			"Arrastra archivos a la ventana para cargarlos. Varios a la vez se acomodan en grilla donde los soltaste. Acepta .frag, .xml, imagenes y videos."),
		N("PNG and GIF transparency is preserved. Drag the media layout slider to choose Custom, Fit, Fill, Stretch or Original. Offset and X/Y scale remain available in every mode; Scale Ratio zooms uniformly while preserving the current aspect ratio.",
			"Se conserva la transparencia de PNG y GIF. Arrastra el selector de layout para elegir Custom, Fit, Fill, Stretch u Original. Offset y escala X/Y siguen disponibles en todos los modos; Scale Ratio aplica zoom uniforme conservando la proporcion actual."),
		N("Animated GIF and video inspectors include transport, IN/OUT range, Once/Loop/Ping-pong, speed and direction. Videos start muted.",
			"Los inspectores de GIF animado y video incluyen transporte, rango IN/OUT, Once/Loop/Ping-pong, velocidad y direccion. Los videos empiezan silenciados."),
		GAP(),

		// ------------------------------------------------------------------
		H("SESSIONS", "SESIONES"),
		E("s", "Save the session into the current XML",
			"Guardar la sesion en el XML actual", Scope::Nodes),
		E("l", "Load the session from the current path",
			"Cargar la sesion desde la ruta actual", Scope::Nodes),
		E("Ctrl+S",
			"Save as: SAVE writes a new file, UPDATE overwrites, CANCEL closes. Works on any screen",
			"Guardar como: SAVE escribe un archivo nuevo, UPDATE sobrescribe, CANCEL cierra. Funciona en cualquier pantalla"),
		N("Careful: in the shader EDITOR, Ctrl+S saves the SHADER FILE instead of the session. It is the only shortcut that means two things.",
			"Ojo: en el EDITOR de shaders, Ctrl+S guarda el ARCHIVO DEL SHADER en vez de la sesion. Es el unico atajo que significa dos cosas."),
		E("t", "Toggle loading as a preset (merge) or as a full session (replace)",
			"Alternar carga como preset (mezcla) o sesion completa (reemplaza)", Scope::Nodes),
		N("The session opened at startup is the Default compo field in SETTINGS.",
			"La sesion que se abre al arrancar es el campo Default compo en SETTINGS."),
		GAP(),

		// ------------------------------------------------------------------
		H("NODE GRAPH", "GRAFO DE NODOS"),
		N("These keys only work on the NODES screen.",
			"Estas teclas solo funcionan en la pantalla NODES.", Scope::Nodes),
		E("Space + drag",
			"Pan the canvas with the left button, as in any design program. "
			"Middle and right drag still pan too. If space seems stuck after "
			"switching windows, tap it once",
			"Mover el lienzo con el boton izquierdo, como en cualquier programa "
			"de diseno. El arrastre medio y derecho siguen funcionando. Si el "
			"space queda trabado despues de cambiar de ventana, tocalo una vez",
			Scope::Nodes),
		E("Ctrl+D",
			"Advanced debug panel: which cameras, Kinect and NDI receivers are "
			"open and how many boxes share each, FBO count and estimated VRAM, "
			"render size, and the crossfade state. Replaces the plain 'd' "
			"readout while it is open",
			"Panel de debug avanzado: que camaras, Kinect y receptores NDI estan "
			"abiertos y cuantas cajas comparten cada uno, cantidad de FBOs y "
			"VRAM estimada, tamano de render, y el estado del crossfade. "
			"Reemplaza el listado de la 'd' mientras esta abierto",
			Scope::Nodes),
		E("Ctrl+C / Ctrl+V", "Copy and paste the selected boxes",
			"Copiar y pegar las cajas seleccionadas", Scope::Nodes),
		E("DEL", "Delete all selected boxes",
			"Eliminar todas las cajas seleccionadas", Scope::Nodes),
		E("u", "Group the selected boxes into one box-group",
			"Agrupar las cajas seleccionadas en un grupo", Scope::Nodes),
		N("Double-click a group tab to rename it: Enter commits, ESC cancels, clicking away commits.",
			"Doble click en la pestana de un grupo para renombrarla: Enter confirma, ESC cancela, click afuera confirma."),
		E("r", "Reload the active shader from disk",
			"Recargar el shader activo desde el disco", Scope::Nodes),
		E("Down / Up Arrow", "Capture the hovered slider's lower / upper limit while custom range is active",
			"Capturar el limite inferior / superior del slider bajo el mouse con el rango activo", Scope::Nodes),
		E("x", "Trigger the code block on the active shader",
			"Disparar el bloque de codigo del shader activo", Scope::Nodes),
		E("e", "Toggle sequence mode",
			"Activar o desactivar el modo secuencia", Scope::Nodes),
		E("z", "Cue the selected box, or the active render of the group you are inside",
			"Poner en cue la caja seleccionada, o el render activo del grupo en el que estas", Scope::Nodes),
		E("w", "Open the separate render window",
			"Abrir la ventana de render aparte", Scope::Nodes),
		E("m", "Save a PNG of the active render into exportimgs/",
			"Guardar un PNG del render activo en exportimgs/", Scope::Nodes),
		E("d", "Toggle the debug overlay: FPS, box count, active compo",
			"Mostrar u ocultar los datos de debug: FPS, cantidad de cajas, compo activa", Scope::Nodes),
		E("Single click", "Select a box and open its inspector",
			"Seleccionar una caja y abrir su inspector", Scope::Nodes),
		E("Double-click", "Make the box the active render",
			"Convertir la caja en el render activo", Scope::Nodes),
		E("Outlet drag", "Connect a box outlet to another box inlet",
			"Conectar la salida de una caja a la entrada de otra", Scope::Nodes),
		E("Empty drag", "Draw a rectangle to select several boxes",
			"Dibujar un rectangulo para seleccionar varias cajas", Scope::Nodes),
		E("Shift + drag",
			"Draw another rectangle and ADD what it touches to the current "
			"selection instead of replacing it",
			"Dibujar otro rectangulo y SUMAR lo que toca a la seleccion actual "
			"en vez de reemplazarla", Scope::Nodes),
		E("Ctrl + click",
			"Add or remove one box from the selection, leaving the rest alone",
			"Sumar o sacar una sola caja de la seleccion, sin tocar el resto",
			Scope::Nodes),
		E("Middle/right drag", "Pan the node canvas",
			"Mover el canvas de nodos", Scope::Nodes),
		E("Mouse wheel", "Zoom the node canvas around the pointer",
			"Hacer zoom del canvas alrededor del puntero", Scope::Nodes),
		GAP(),

		// ------------------------------------------------------------------
		H("ADDING BOXES", "AGREGAR CAJAS"),
		N("Also NODES only. IMPORT (4) is the browsable way to add shaders.",
			"Tambien solo en NODES. IMPORT (4) es la forma navegable de agregar shaders.", Scope::Nodes),
		E("c", "Camera input", "Entrada de camara", Scope::Nodes),
		E("i", "Frame difference", "Diferencia de cuadros", Scope::Nodes),
		E("b", "Paint canvas: draw by hand, animate cel by cel",
			"Lienzo de dibujo: dibuja a mano, anima cuadro por cuadro",
			Scope::Nodes),
#ifdef NDI
		E("n", "NDI receiver", "Receptor NDI", Scope::Nodes),
#endif
#ifdef SPOUT
		E("h", "Spout input", "Entrada Spout", Scope::Nodes),
#endif
		E("Shift+C", "Kinect V2 input", "Entrada Kinect V2", Scope::Nodes),
		E("Shift+D", "Camera depth: pseudo-depth from an ordinary camera",
			"Camara profundidad: pseudo-profundidad desde una camara comun",
			Scope::Nodes),
		E("Shift+P", "PointerCloud: Kinect V2 point cloud",
			"PointerCloud: nube de puntos del Kinect V2", Scope::Nodes),
		GAP(),

		// ------------------------------------------------------------------
		H("PAINT CANVAS EDITOR", "EDITOR DEL LIENZO DE DIBUJO"),
		N("Add a paint box with b, select it, then press PAINT in the inspector header. These keys are live only while that panel is open.",
			"Agrega una caja paint con b, seleccionala y presiona PAINT en el encabezado del inspector. Estas teclas andan solo con ese panel abierto.",
			Scope::Paint),
		E("b / e", "Brush / eraser", "Pincel / goma", Scope::Paint),
		E("l / r / o", "Line / rectangle / ellipse",
			"Linea / rectangulo / elipse", Scope::Paint),
		E("p", "Pen: draw an open sweep and it closes to its start point and fills on release",
			"Pluma: dibuja un trazo abierto y al soltar se cierra con el punto inicial y se rellena",
			Scope::Paint),
		E("g", "Fill: click a region to flood it. The size slider becomes the tolerance while this tool is selected",
			"Relleno: clic en una zona para rellenarla. El deslizador de tamano pasa a ser la tolerancia con esta herramienta",
			Scope::Paint),
		E("s / m", "Free / rectangular selection: clips without deforming; Shift adds and Alt subtracts. Drag to move, use the top handle to rotate or corners to scale. DEL removes it and D duplicates it",
			"Seleccion libre / rectangular: recorta sin deformar; Shift suma y Alt resta. Arrastra para mover, usa la manija superior para rotar o las esquinas para escalar. DEL borra y D duplica",
			Scope::Paint),
		E("Ctrl+A", "Select the whole active layer. Enter confirms and Esc deselects",
			"Seleccionar toda la capa activa. Enter confirma y Esc deselecciona",
			Scope::Paint),
		E("Ctrl+C / X / V", "Copy, cut and paste the selection, including between layers and cels",
			"Copiar, cortar y pegar la seleccion, incluso entre capas y cuadros",
			Scope::Paint),
		E("Alt+right click", "Eyedropper: samples the colour under the cursor without leaving the tool you are holding",
			"Cuentagotas: toma el color bajo el cursor sin salir de la herramienta que tenes en la mano",
			Scope::Paint),
		E("Hex", "The field at the bottom of the colour picker takes #RGB, #RRGGBB or #RRGGBBAA. Enter applies, Esc cancels, and it shows the current colour when not being edited",
			"El campo al pie del selector acepta #RGB, #RRGGBB o #RRGGBBAA. Enter aplica, Esc cancela, y muestra el color actual cuando no lo estas editando",
			Scope::Paint),
		E("[ / ]", "Brush size down / up",
			"Reducir / aumentar el tamano del pincel", Scope::Paint),
		E("Ctrl+Z", "Undo. Add Shift to redo",
			"Deshacer. Con Shift, rehacer", Scope::Paint),
		E(", / .", "Previous / next cel", "Cuadro anterior / siguiente",
			Scope::Paint),
		E("Space", "Play or pause the animation",
			"Reproducir o pausar la animacion", Scope::Paint),
		E("Direction", "The arrow in the transport row plays forwards or backwards. Ping-pong flips it as it bounces; changing the playback mode restores the direction you chose",
			"La flecha en la fila de transporte reproduce hacia adelante o atras. Ping-pong la invierte al rebotar; al cambiar el modo se restaura la direccion que elegiste",
			Scope::Paint),
		E("n / d", "New cel / duplicate the current cel",
			"Cuadro nuevo / duplicar el cuadro actual", Scope::Paint),
		E("Del", "Delete the current cel, never the box",
			"Borrar el cuadro actual, nunca la caja", Scope::Paint),
		E("- / =", "Shorten or lengthen how long this cel is held",
			"Acortar o alargar cuanto dura este cuadro", Scope::Paint),
		E("Shift+O", "Cycle the onion skin range",
			"Cambiar el rango del papel cebolla", Scope::Paint),
		E("< / >", "Select the layer below / above",
			"Seleccionar la capa de abajo / de arriba", Scope::Paint),
		E("Double click", "On a layer's name, rename it. Enter applies, Esc cancels",
			"Sobre el nombre de una capa, renombrarla. Enter aplica, Esc cancela",
			Scope::Paint),
		E("Timeline", "Rows are layers, columns are frames, like Aseprite. Click a cell to select that frame AND that layer; a filled marker means the layer has strokes there. All layers always share the same frames",
			"Las filas son capas y las columnas cuadros, como en Aseprite. Clic en una celda selecciona ese cuadro Y esa capa; un marcador lleno indica que la capa tiene trazos ahi. Todas las capas comparten siempre los mismos cuadros",
			Scope::Paint),
		E("Drag", "A frame number reorders frames, a layer row reorders layers. The wheel scrolls frames over the grid and layers over the left gutter",
			"Un numero de cuadro reordena cuadros, una fila de capa reordena capas. La rueda desplaza cuadros sobre la grilla y capas sobre la columna izquierda",
			Scope::Paint),
		E("BG", "Marks a layer as the background: it is drawn from one shared set of strokes on EVERY frame, so a static backdrop is drawn once instead of copied onto each frame. Its row shows as a single band rather than per-frame markers",
			"Marca una capa como fondo: se dibuja desde un solo conjunto de trazos en TODOS los cuadros, asi un fondo fijo se dibuja una vez en lugar de copiarse en cada cuadro",
			Scope::Paint),
		E("?", "This list. Esc closes it, Esc again closes the panel",
			"Esta lista. Esc la cierra, Esc otra vez cierra el panel", Scope::Paint),
		E("DEL / BACKSPACE", "Clears the current cel, or deletes selected strokes if a selection is active",
			"Limpia la celda actual, o borra los trazos seleccionados si hay una selección activa",
			Scope::Paint),
		E("Shift + DEL", "Deletes the current frame from the timeline",
			"Borra el cuadro actual de la línea de tiempo",
			Scope::Paint),
		E("Right click", "On a palette swatch, removes it from the palette",
			"Sobre un color de la paleta, lo quita",
			Scope::Paint),

		E("Palette", "Click the colour swatch to open the picker, then + to save the current colour. The palette is kept in data/paint_palette.xml and survives restarts",
			"Clic en el color abre el selector, y + guarda el color actual. La paleta se guarda en data/paint_palette.xml y sobrevive reinicios",
			Scope::Paint),
		E("Ctrl+drag", "Pan the canvas. Middle drag does the same, scroll to zoom",
			"Mover el lienzo. El boton del medio hace lo mismo, rueda para acercar",
			Scope::Paint),
		N("The texture input is a tracing reference: it shows in the editor and is never part of what the box outputs.",
			"La entrada de textura es una referencia para calcar: se ve en el editor y nunca forma parte de lo que la caja saca.",
			Scope::Paint),
		GAP(),

		// ------------------------------------------------------------------
		H("IMPORT: SHADER BROWSER", "IMPORT: NAVEGADOR DE SHADERS"),
		N("Press 4. The search field takes focus straight away, so letter shortcuts are off until you press ESC.",
			"Presiona 4. El campo de busqueda toma el foco enseguida, asi que los atajos de letras no andan hasta que presiones ESC.", Scope::Import),
		E("Type", "Filter the shader list",
			"Filtrar la lista de shaders", Scope::Import),
		E("Up / Down", "Move the selection",
			"Mover la seleccion", Scope::Import),
		E("Enter", "Load the selected shader as a box. Double-click does the same",
			"Cargar el shader seleccionado como caja. Doble click hace lo mismo", Scope::Import),
		E("Star icon", "Mark a favourite. Favourites are kept in data/shader_favorites.xml",
			"Marcar favorito. Los favoritos se guardan en data/shader_favorites.xml", Scope::Import),
		E("MOVE MIDI", "Bind a MIDI control that adds this shader, without opening the MIDI screen",
			"Asignar un control MIDI que agrega este shader, sin abrir la pantalla MIDI", Scope::Import),
		GAP(),

		// ------------------------------------------------------------------
		H("SHADER EDITOR", "EDITOR DE SHADERS"),
		N("Press 5 or the EDITOR tab to edit the selected shader. Open files appear as tabs.",
			"Presiona 5 o la pestana EDITOR para editar el shader seleccionado. Los archivos abiertos aparecen como pestanas.", Scope::Editor),
		E("Ctrl+S", "Save the shader file. This does NOT save the session",
			"Guardar el archivo del shader. Esto NO guarda la sesion", Scope::Editor),
		E("Ctrl+C / X / V / A", "Copy, cut, paste, and select all inside the text",
			"Copiar, cortar, pegar y seleccionar todo dentro del texto", Scope::Editor),
		E("Home / End", "Start and end of the line. PageUp and PageDown scroll",
			"Inicio y fin de linea. PageUp y PageDown scrollean", Scope::Editor),
		E("Shift + arrows", "Select text",
			"Seleccionar texto", Scope::Editor),
		E("r", "Back on NODES, reload the shader to see your changes",
			"De vuelta en NODES, recarga el shader para ver los cambios", Scope::Nodes),
		GAP(),

		// ------------------------------------------------------------------
		H("MIDI MAPPING", "MAPEO MIDI"),
		N("Press 6 or the MIDI tab. Bindings are saved per device, so two controllers keep separate maps.",
			"Presiona 6 o la pestana MIDI. Las asignaciones se guardan por dispositivo, asi dos controladores mantienen mapas separados.", Scope::Midi),
		E("Key bind map on/off",
			"Master switch for mapping mode. While it is on a badge shows in the top bar",
			"Interruptor general del modo mapeo. Mientras esta activo se ve un cartel en la barra superior", Scope::Midi),
		E("Learn",
			"Arm a row, then move a MIDI control to bind it",
			"Arma una fila y despues mueve un control MIDI para asignarlo", Scope::Midi),
		N("If that control is already bound you get a prompt: Replace, Keep both, or Cancel. Keep both fires every binding on the key.",
			"Si ese control ya esta asignado aparece un aviso: Replace, Keep both o Cancel. Keep both dispara todas las asignaciones de esa tecla.", Scope::Midi),
		E("Target box + Action",
			"Build a custom bind: pick a box, an action, and for Parameter also which parameter",
			"Armar una asignacion propia: elegi una caja, una accion y, para Parameter, tambien que parametro", Scope::Midi),
		E("Rescan devices",
			"Pick up a controller that was plugged in after startup",
			"Detectar un controlador conectado despues de arrancar", Scope::Midi),
		N("Global actions include next and previous box, set active, toggle gallery mode, and BPM tap. Gallery mode is only reachable from a MIDI binding.",
			"Las acciones globales incluyen caja siguiente y anterior, set active, modo galeria y BPM tap. El modo galeria solo se alcanza desde una asignacion MIDI.", Scope::Midi),
		N("With mapping mode on you can also click a box button or an inspector slider directly to arm it.",
			"Con el modo mapeo activo tambien podes hacer click directo en un boton de caja o en un slider del inspector para armarlo.", Scope::Midi),
		GAP(),

		// ------------------------------------------------------------------
		H("CUE AND PROJECTION MAPPING", "CUE Y MAPEO DE PROYECCION"),
		N("Cue stages a box so you can set it up before it goes live. The cue panel shows it in amber while the output keeps running.",
			"El cue prepara una caja para que la ajustes antes de que salga al aire. El panel de cue la muestra en ambar mientras la salida sigue corriendo."),
		E("CUE / z", "Open the cue panel and cue the selected box",
			"Abrir el panel de cue y poner en cue la caja seleccionada"),
		E("MAP", "Projection mapping editor. Needs a mapping shader selected first",
			"Editor de mapeo de proyeccion. Necesita un shader de mapeo seleccionado"),
		N("In the mapping editor drag the corners to fit the surface, and use the buttons to show borders, points and the curved grid.",
			"En el editor de mapeo arrastra las esquinas para ajustar la superficie, y usa los botones para mostrar bordes, puntos y la grilla curva."),
		E("Mask pen", "Click to add points, click the first point to close, click a closed edge to insert a point, and right-click a point to delete it",
			"Click para agregar puntos, click en el primero para cerrar, click en un borde cerrado para insertar un punto y click derecho para borrarlo"),
		E("New Shape (+)", "After closing the selected mask, start another independent mask shape on the same texture layer",
			"Despues de cerrar la mascara seleccionada, empezar otra forma de mascara independiente en la misma textura"),
		E("MOVE target icons", "The highlighted Mesh or Pen icon shows whether MOVE targets the surface or masks. Click either icon while moving to switch target",
			"El icono Mesh o Pen resaltado muestra si MOVE apunta a la superficie o las mascaras. Click en un icono durante MOVE para cambiar objetivo"),
		E("Mask selection", "Click a mask to select it, Shift-click to toggle it, or drag empty space for a marquee; Shift-marquee adds masks",
			"Click en una mascara para seleccionarla, Shift-click para alternarla, o arrastra espacio vacio para un marco; Shift-marco agrega mascaras"),
		E("Mask transform", "Drag inside a selected mask to move the group, use corners to scale uniformly, or the curved handle to rotate; Shift snaps rotation to 15 degrees",
			"Arrastra dentro de una mascara seleccionada para mover el grupo, usa esquinas para escalar uniforme o el control curvo para rotar; Shift ajusta a 15 grados"),
		E("Mapping wheel", "Zoom the advanced-mapping preview around the pointer from 100% to 1600%",
			"Hacer zoom del preview de mapeo avanzado alrededor del puntero, de 100% a 1600%"),
		E("Right drag / Middle drag", "Pan the mapping preview in any tool without changing geometry. A stationary right-click on a Pen point deletes it",
			"Mover el preview en cualquier herramienta sin cambiar geometria. Un click derecho quieto en un punto Pen lo borra"),
		E("Zoom %", "Click the percentage in the mapping header to fit and center the preview",
			"Click en el porcentaje del encabezado para ajustar y centrar el preview"),
		GAP(),

		// ------------------------------------------------------------------
		H("SETTINGS AND OUTPUT", "CONFIGURACION Y SALIDA"),
		N("Press 2. Holds OSC ports and IP, render size, BPM with AUTOTAP, Spout and NDI toggles, and the default compo.",
			"Presiona 2. Tiene los puertos e IP de OSC, tamano de render, BPM con AUTOTAP, Spout y NDI, y la compo por defecto.", Scope::Settings),
		E("Live outputs",
			"Add an output per screen, choose its monitor and source, and crop it",
			"Agregar una salida por pantalla, elegir monitor y fuente, y recortarla", Scope::Settings),
		E("Fullscreen",
			"A checkbox on each live output. This replaced the old global f shortcut",
			"Una casilla en cada salida. Reemplazo al viejo atajo global f", Scope::Settings),
		E("Screen wall",
			"Lay several outputs out as a wall, by grid or by measured positions in mm",
			"Acomodar varias salidas como muro, por grilla o por posiciones medidas en mm", Scope::Settings),
		E("Tab / Shift+Tab", "Move between fields while editing a live output or wall split",
			"Moverse entre campos al editar una salida o division del muro", Scope::Settings),
		GAP(),

		// ------------------------------------------------------------------
		H("OSC", "OSC"),
		N("Incoming messages use the address shown below plus one numeric argument. Commands that do not use the value still require a numeric argument.",
			"Los mensajes entrantes usan la direccion indicada y un argumento numerico. Los comandos que no usan el valor igualmente requieren un argumento numerico."),
		E("/load/<file>", "Load savefiles/<file>; the numeric argument is ignored",
			"Cargar savefiles/<archivo>; el argumento numerico se ignora"),
		E("/setactiverender", "Argument: box index to make active in the current graph",
			"Argumento: indice de la caja a activar en el grafo actual"),
		E("/nextshader", "Select the next box",
			"Seleccionar la caja siguiente"),
		E("/prevshader", "Select the previous box",
			"Seleccionar la caja anterior"),
		E("/setactiveshader", "Make the currently selected box active",
			"Convertir la caja seleccionada en activa"),
		E("/nextshader_gallerymode", "Select and activate the next box",
			"Seleccionar y activar la caja siguiente"),
		E("/prevshader_gallerymode", "Select and activate the previous box",
			"Seleccionar y activar la caja anterior"),
		E("/setactivecycle", "Toggle sequence/gallery mode",
			"Alternar el modo secuencia/galeria"),
		E("/disablegallerymode", "Disable gallery mode and enable every box",
			"Desactivar modo galeria y encender todas las cajas"),
		E("/setdurationgalleryms", "Argument: gallery duration in milliseconds",
			"Argumento: duracion de galeria en milisegundos"),
		E("/addmirrorsquad", "Add the mirrorquad shader box",
			"Agregar la caja del shader mirrorquad"),
		E("/<box>/<parameter>", "Argument: native value for a named float parameter; use /<box>/onoff for its power state",
			"Argumento: valor nativo de un parametro float por nombre; usa /<caja>/onoff para encenderla"),
		E("/openguinumber/<parameterIndex>", "Argument: value for that parameter on the currently open inspector",
			"Argumento: valor para ese parametro en el inspector actualmente abierto"),
		GAP(),

		// ------------------------------------------------------------------
		H("GLOBAL SHADER UNIFORMS", "UNIFORMES GLOBALES DE SHADER"),
		N("Declare any of these in a .frag and Guipper fills it in every frame.",
			"Declara cualquiera de estos en un .frag y Guipper lo completa en cada cuadro."),
		E("uniform float time;", "Seconds since the app started",
			"Segundos desde que arranco el programa"),
		E("uniform vec2 resolution;", "Render size in pixels",
			"Tamano del render en pixeles"),
		E("uniform float bpm;", "Global BPM, shared with AUTOTAP and BPM tap",
			"BPM global, compartido con AUTOTAP y BPM tap"),
		E("uniform vec4 mouse;", "Normalized main-window mouse; xy are current and zw store the last click",
			"Mouse normalizado de la ventana principal; xy son actuales y zw guardan el ultimo click"),
		E("uniform vec2 window_mouse;", "Legacy normalized render-window mouse value; display-only live outputs no longer update it",
			"Valor normalizado legacy del mouse de render; las salidas solo-display ya no lo actualizan"),
		E("uniform int globalframeNum;", "Frames since the app started",
			"Cuadros desde que arranco el programa"),
		E("uniform int boxframeNum;", "Frames since this box started",
			"Cuadros desde que arranco esta caja"),
		E("uniform sampler2D feedback;", "This box's previous frame, for feedback effects",
			"El cuadro anterior de esta caja, para efectos de feedback"),
		GAP(),

		// ------------------------------------------------------------------
		H("AUDIO", "AUDIO"),
		N("Turn on the audio input in SETTINGS and pick a device. The meter there shows what is being heard.",
			"Enciende la entrada de audio en SETTINGS y elegi un dispositivo. El medidor de ahi muestra lo que se escucha.", Scope::Settings),
		N("Choose MIX, LEFT or RIGHT for stereo interfaces. AUTO GAIN adapts to program level; CALIBRATE listens for three seconds to learn the room noise floor.",
			"Elegi MIX, LEFT o RIGHT para interfaces estereo. AUTO GAIN se adapta al nivel; CALIBRATE escucha tres segundos para aprender el ruido ambiente.", Scope::Settings),
		N("On any slider, the audio button makes the parameter follow the sound. The chip next to it picks the source, and the slider's own range handles set how far it travels.",
			"En cualquier slider, el boton de audio hace que el parametro siga al sonido. El chip de al lado elige la fuente, y los manejadores de rango del slider definen cuanto se mueve."),
		N("Open SHAPE for amount, normal/invert polarity, threshold, curve, attack and release. All envelope times are milliseconds and remain consistent at any frame rate.",
			"Abri SHAPE para cantidad, polaridad normal/invertida, umbral, curva, ataque y release. Los tiempos estan en milisegundos y no dependen de los FPS."),
		E("LOW MID HGH", "Frequency bands",
			"Bandas de frecuencia"),
		E("KIK SNR", "Kick and snare hits",
			"Golpes de bombo y redoblante"),
		E("LBAS HMID", "Blends: low+kick, high+snare. Steadier drivers than a band alone",
			"Mezclas: grave+bombo, agudo+redoblante. Mas estables que una banda sola"),
		E("LVL", "Overall level", "Nivel general"),
		E("TRGk TRGs", "Trigger: a pulse on the counted beat",
			"Trigger: un pulso en el beat contado"),
		E("EXPk EXPs", "Express: holds the value until the next count",
			"Express: mantiene el valor hasta el siguiente conteo"),
		E("LOGk LOGs", "Logic: a toggle that flips on the counted beat",
			"Logic: un interruptor que cambia en el beat contado"),
		N("For the three rhythm shapes a second chip picks how often they fire: every 1, 2, 4, 8 or 16 hits.",
			"Para las tres formas ritmicas un segundo chip elige cada cuanto disparan: cada 1, 2, 4, 8 o 16 golpes."),
		N("A parameter following audio ignores MIDI, exactly as BPM sync does.",
			"Un parametro que sigue al audio ignora el MIDI, igual que la sincronia con BPM."),
		GAP(),
		E("uniform vec4 audio_bands;", "x low, y mid, z high, w level",
			"x grave, y medio, z agudo, w nivel"),
		E("uniform vec4 audio_hits;", "x kick, y snare, z low+kick, w high+snare",
			"x bombo, y redoblante, z grave+bombo, w agudo+redoblante"),
		E("uniform float audio_trigger;", "Pulse on the counted beat, division set in SETTINGS",
			"Pulso en el beat contado, division definida en SETTINGS"),
		E("uniform float audio_express;", "Held value until the next counted beat",
			"Valor mantenido hasta el siguiente beat contado"),
		E("uniform float audio_logic;", "Toggle that flips on the counted beat",
			"Interruptor que cambia en el beat contado"),
		E("uniform vec4 audio_onsets;", "x kick trigger, y snare trigger, z kick logic, w snare logic; normalized 0..1",
			"x trigger bombo, y trigger redoblante, z logica bombo, w logica redoblante; normalizado 0..1"),
		E("uniform vec4 audio_rhythm;", "x beat phase 0..1, y beat pulse 0/1, z detected BPM, w confidence 0..1",
			"x fase 0..1, y pulso 0/1, z BPM detectado, w confianza 0..1"),
		E("uniform vec4 audio_spectrum0..3;", "Sixteen normalized log-frequency bins, four bins per vec4",
			"Dieciseis bandas logaritmicas normalizadas, cuatro por vec4"),
		};
		return t;
	}
}
