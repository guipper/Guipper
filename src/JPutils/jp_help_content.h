#pragma once

// No openFrameworks: this is a table of literals, and staying free of it is what
// lets tests/ compile the table and check every keyboard annotation against the
// board. The include was a leftover - the only oF names left in the file are two
// mentions inside comments.
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
// Both languages are UTF-8. The UI fonts explicitly load the Latin range, so
// Spanish punctuation and accents are part of the contract rather than copy
// that only happens to work on one font backend.
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
		// Which PHYSICAL keys this row lights up on the keyboard map, as ids
		// from jp_keymap::board() - "ctrl+shift+g", "b e", "space".
		//
		// Stated, never inferred. `keys` above is prose for a human: it holds
		// "Bucket", "/setactiverender" and "uniform vec4 audio_bands;" as often
		// as it holds a keystroke, and where it does hold one the "/" means five
		// different things. Reading keys out of it would be the same mistake
		// this file's header says was removed.
		//
		// Empty is the default and means "not on the map", which is correct for
		// the 78 rows that name a widget, an OSC address or a GLSL declaration.
		const char *phys = "";
	};

	inline Line H(const char *en, const char *es)
	{
		return {Kind::Heading, Scope::Global, "", en, es, ""};
	}
	inline Line N(const char *en, const char *es, Scope s = Scope::Global)
	{
		return {Kind::Note, s, "", en, es, ""};
	}
	inline Line S(const char *number, const char *en, const char *es)
	{
		return {Kind::Step, Scope::Global, number, en, es, ""};
	}
	inline Line GAP()
	{
		return {Kind::Gap, Scope::Global, "", "", "", ""};
	}
	inline Line E(const char *keys, const char *en, const char *es,
		Scope s = Scope::Global, const char *phys = "")
	{
		return {Kind::Entry, s, keys, en, es, phys};
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

	// The selected language, so every reader of this table agrees on it.
	//
	// The paint panel's shortcuts modal used to hardcode Spanish while the HELP
	// screen had a working ES/EN toggle, so the same rows appeared in two
	// languages depending on where you read them.
	inline int &languageRef()
	{
		static int selected = 0;
		return selected;
	}
	inline void setLanguage(int language) { languageRef() = language; }
	inline int language() { return languageRef(); }

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
		// Before any shortcut: what the thing IS. QUICK START used to open the
		// screen, and its first step already assumed you knew what a box, an
		// outlet and the active render were.
		H("WHAT GUIPPER IS", "QUÉ ES GUIPPER"),
		N("Guipper creates live visuals by linking small image processors. Add a source—video, image or camera—pass it through effects, and send the result to a screen or projector. Everything happens in real time, without a render step or timeline.",
			"Guipper crea visuales en vivo conectando pequeños procesadores de imagen. Agrega una fuente —video, imagen o cámara—, pásala por efectos y envía el resultado a una pantalla o proyector. Todo sucede en tiempo real, sin render previo ni línea de tiempo."),
		N("A BOX is one step in that chain. A source box creates an image; an effect box receives one, provides controls to transform it, and returns the result.",
			"Una CAJA es un paso de esa cadena. Una caja de fuente genera una imagen; una caja de efecto la recibe, ofrece controles para transformarla y devuelve el resultado."),
		N("Connect boxes from the OUTLET on the right to an INLET on the left. The cable carries the source image into the next box.",
			"Conecta las cajas desde la SALIDA de la derecha hacia una ENTRADA de la izquierda. El cable lleva la imagen de la fuente hasta la caja siguiente."),
		N("One box is the ACTIVE RENDER, marked with a number. Its image is sent to the outputs. Double-click a box to make it active.",
			"Una caja es el RENDER ACTIVO, marcado con un número. Su imagen se envía a las salidas. Haz doble clic en una caja para convertirla en activa."),
		N("The complete arrangement is a COMPOSITION stored in one .xml file. Save it as a session and use Undo to revert graph edits.",
			"El armado completo es una COMPOSICIÓN guardada en un único archivo .xml. Guárdala como sesión y usa Deshacer para revertir ediciones del grafo."),
		GAP(),

		// ------------------------------------------------------------------
		H("QUICK START", "INICIO RÁPIDO"),
		S("1", "Add a source: drag an image or video into the window, or add Camera/Kinect from NODES.",
			"Agrega una fuente: arrastra una imagen o video a la ventana, o añade Camera/Kinect desde NODES."),
		S("2", "Open IMPORT (4), search for an effect, and load it as a box.",
			"Abre IMPORT (4), busca un efecto y cárgalo como caja."),
		S("3", "Drag from the source OUTLET to the effect INLET to connect them.",
			"Arrastra desde la SALIDA de la fuente hasta la ENTRADA del efecto para conectarlas."),
		S("4", "Select a box to edit it in the inspector. Double-click it to make it the active render.",
			"Selecciona una caja para editarla en el inspector. Haz doble clic para convertirla en el render activo."),
		S("5", "Configure outputs and BPM in SETTINGS, map live controls in MIDI, and use CUE to prepare changes off-air.",
			"Configura las salidas y el BPM en SETTINGS, asigna controles en MIDI y usa CUE para preparar cambios fuera del aire."),
		GAP(),

		// ------------------------------------------------------------------
		H("GETTING AROUND", "MOVERSE POR EL PROGRAMA"),
		E("1 - 6",
			"Switch screens: 1 NODES, 2 SETTINGS, 3 HELP, 4 IMPORT, 5 EDITOR, 6 MIDI",
			"Cambia de pantalla: 1 NODES, 2 SETTINGS, 3 HELP, 4 IMPORT, 5 EDITOR, 6 MIDI", Scope::Global, "1 2 3 4 5 6"),
		N("The top bar provides the same six screens as buttons. CUE and MAP form a separate group for staging and projection mapping.",
			"La barra superior ofrece las mismas seis pantallas como botones. CUE y MAP forman un grupo separado para preparación y mapping de proyección."),
		E("CUE",
			"Toggle cue for the selected box; with no selection, use the active render",
			"Activa o desactiva el cue de la caja seleccionada; si no hay selección, usa el render activo"),
		E("MAP",
			"Open or close projection mapping for the selected mapping shader",
			"Abre o cierra el mapping de proyección del shader de mapping seleccionado"),
		E("ESC",
			"Close the topmost surface, one layer per press: field, menu, panel or modal. It never switches screens",
			"Cierra la superficie superior, una capa por pulsación: campo, menú, panel o modal. Nunca cambia de pantalla", Scope::Global, "esc"),
		E("Enter",
			"Confirm the focused text field; ESC cancels the edit",
			"Confirma el campo de texto enfocado; ESC cancela la edición", Scope::Global, "enter"),
		E("Mouse wheel",
			"Scroll the list under the pointer. HELP has separate document and index scrolling; IMPORT, MIDI and SETTINGS scroll their own lists",
			"Desplaza la lista bajo el puntero. HELP tiene scroll separado para el documento y el índice; IMPORT, MIDI y SETTINGS desplazan sus propias listas"),
		N("Drop files into the window to load them. Multiple files are arranged in a grid around the drop point. Supported inputs include .frag, .xml, PNG, JPG, JPEG, GIF, MOV, MKV, MP4, FLV, VOB and AVI.",
			"Suelta archivos en la ventana para cargarlos. Si sueltas varios, se ordenan en una grilla alrededor del punto de caída. Se admiten .frag, .xml, PNG, JPG, JPEG, GIF, MOV, MKV, MP4, FLV, VOB y AVI."),
		N("PNG and GIF alpha is preserved. Use the media layout control to select Custom, Fit, Fill, Stretch or Original. Offset and X/Y scale remain available in every mode; Scale Ratio zooms uniformly without changing the current proportion.",
			"Se conserva la transparencia de PNG y GIF. Usa el control de layout para elegir Custom, Fit, Fill, Stretch u Original. Offset y escala X/Y siguen disponibles en todos los modos; Scale Ratio aplica zoom uniforme sin alterar la proporción actual."),
		E("IN / OUT field", "Select a time field and enter seconds (12.5), minutes and seconds (1:30), hours (0:01:30), or a frame followed by f (90f). Enter confirms; ESC cancels. The first typed key replaces the previous value",
			"Selecciona un campo de tiempo y escribe segundos (12.5), minutos y segundos (1:30), horas (0:01:30) o un cuadro seguido de f (90f). Enter confirma; ESC cancela. La primera tecla escrita reemplaza el valor anterior",
			Scope::Nodes, "enter esc"),
		E("Up / Down", "With a time field focused, move it by one frame. IN never crosses OUT: moving either endpoint past the other carries the other along",
			"Con un campo de tiempo enfocado, muévelo un cuadro. IN nunca cruza OUT: si desplazas un extremo más allá del otro, el segundo avanza con él",
			Scope::Nodes, "up down"),
		N("Animated GIF and video inspectors provide transport, IN/OUT range, Once/Loop/Ping-pong, speed and direction. Videos start muted.",
			"Los inspectores de GIF animado y video incluyen transporte, rango IN/OUT, Once/Loop/Ping-pong, velocidad y dirección. Los videos comienzan silenciados."),
		E("Right click on a cycling button", "Any button that advances through a list of values on click steps BACK on right click: audio source, audio division, BPM rate, automation pattern, Once/Loop/Ping-pong, layer blend, symmetry and onion skin, and the cycling buttons in SETTINGS",
			"Cualquier botón que avance por una lista de valores al hacer clic RETROCEDE con el clic derecho: fuente de audio, división de audio, BPM rate, patrón de automatización, Once/Loop/Ping-pong, blend de capa, simetría y papel cebolla, y los botones cíclicos de SETTINGS"),
		GAP(),

		// ------------------------------------------------------------------
		H("SESSIONS", "SESIONES"),
		E("s", "Save directly to the current session XML",
			"Guarda directamente en el XML de la sesión actual", Scope::Nodes, "s"),
		E("l", "Load the session from the current path",
			"Carga la sesión desde la ruta actual", Scope::Nodes, "l"),
		E("Ctrl/Cmd+S", "Open the system Save As dialog for the composition. In EDITOR, save the shader file instead",
			"Abre el diálogo Guardar como del sistema para la composición. En EDITOR, guarda el archivo del shader", Scope::Global, "ctrl+s"),
		E("Ctrl/Cmd+Shift+S", "Open the in-app save dialog: SAVE creates a file, UPDATE replaces the current one, and CANCEL closes the dialog",
			"Abre el guardado interno: SAVE crea un archivo, UPDATE reemplaza el actual y CANCEL cierra el diálogo", Scope::Global, "ctrl+shift+s"),
		N("Context resolves overlapping chords automatically. Ctrl/Cmd+Shift+G ungroups in NODES but exports a GIF while Paint is open; Ctrl/Cmd+Z undoes Paint while drawing and the graph otherwise.",
			"El contexto resuelve automáticamente los acordes compartidos. Ctrl/Cmd+Shift+G desagrupa en NODES, pero exporta un GIF con Paint abierto; Ctrl/Cmd+Z deshace Paint mientras dibujas y el grafo en los demás casos."),
		E("t", "Toggle loading as a preset (merge) or as a full session (replace)",
			"Alterna entre cargar como preset —combinar— o como sesión completa —reemplazar—", Scope::Nodes, "t"),
		N("The session opened at startup is the Default compo field in SETTINGS.",
			"La sesión que se abre al iniciar se define en el campo Default compo de SETTINGS."),
		GAP(),

		// ------------------------------------------------------------------
		H("NODE GRAPH", "GRAFO DE NODOS"),
		N("These keys only work on the NODES screen.",
			"Estas teclas solo funcionan en la pantalla NODES.", Scope::Nodes),
		E("Space + drag",
			"Pan the canvas with the left button. Middle and right drag also pan. If Space remains held after switching windows, tap it once",
			"Desplaza el lienzo con el botón izquierdo. El botón central y el derecho también desplazan. Si Space queda activo después de cambiar de ventana, púlsalo una vez",
			Scope::Nodes, "space"),
		E("Ctrl+D",
			"Toggle the advanced diagnostic panel: shared Camera/Kinect/NDI sources, FBO count, estimated VRAM, render size and transition state. It replaces the compact d overlay while open",
			"Muestra u oculta el diagnóstico avanzado: fuentes Camera/Kinect/NDI compartidas, cantidad de FBO, VRAM estimada, tamaño de render y estado de transición. Mientras está abierto reemplaza el resumen de d",
			Scope::Nodes, "ctrl+d"),
		E("Ctrl/Cmd+C / V", "Copy and paste the selected boxes",
			"Copia y pega las cajas seleccionadas", Scope::Nodes, "ctrl c v"),
		E("DEL / BACKSPACE", "Delete every selected box",
			"Elimina todas las cajas seleccionadas", Scope::Nodes, "del backspace"),
		E("Ctrl/Cmd+G", "Group at least two selected boxes. Their live instances move into the group without recompiling or reloading",
			"Agrupa al menos dos cajas seleccionadas. Sus instancias activas pasan al grupo sin recompilarse ni recargarse", Scope::Nodes, "ctrl+g"),
		E("Ctrl/Cmd+Shift+G", "Dissolve every selected group into the current view as one undo step. Its boxes keep their live state",
			"Disuelve todos los grupos seleccionados en la vista actual como un único paso de deshacer. Las cajas conservan su estado activo", Scope::Nodes, "ctrl+shift+g"),
		E("Ctrl/Cmd+Z", "Undo the last graph edit. Add Shift to redo. Covers moving, deleting, connecting, grouping, parameter values, box state, the active render, and grouping in both directions. One history for the whole composition: a step taken inside a group is undone from anywhere, and the canvas goes there to show it. The mapping panel keeps its own. A knob sweep over MIDI is one step, not one per message. Adding a box, MIDI bindings, zoom and pan are not recorded",
			"Deshace la última edición del grafo; añade Shift para rehacer. Incluye movimiento, borrado, conexiones, agrupación, parámetros, estado de cajas y render activo. La composición comparte un historial y el lienzo navega hasta el lugar del cambio. Mapping mantiene uno propio. Un barrido MIDI cuenta como un paso; agregar cajas, bindings MIDI, zoom y desplazamiento no se registran", Scope::Nodes, "ctrl+z"),
		N("Double-click a group tab to rename it: Enter commits, ESC cancels, clicking away commits.",
			"Haz doble clic en la pestaña de un grupo para renombrarla: Enter confirma, ESC cancela y un clic fuera confirma."),
		E("r", "Reload the selected shader from disk; with no selection, reload the active render",
			"Recarga desde disco el shader seleccionado; si no hay selección, recarga el render activo", Scope::Nodes, "r"),
		E("Down / Up Arrow", "Capture the hovered slider's lower / upper limit while custom range is active",
			"Captura el límite inferior o superior del slider bajo el puntero cuando el rango personalizado está activo", Scope::Nodes, "down up"),
		E("x", "Trigger the code block on the active shader",
			"Dispara el bloque de código del shader activo", Scope::Nodes, "x"),
		E("e", "Toggle sequence mode",
			"Activa o desactiva el modo secuencia/galería", Scope::Nodes, "e"),
		E("z", "Toggle cue for the selected box; with no selection, use the active render in the current graph",
			"Activa o desactiva el cue de la caja seleccionada; si no hay selección, usa el render activo del grafo actual", Scope::Nodes, "z"),
		E("w", "Open the separate render window",
			"Abre la ventana de render independiente", Scope::Nodes, "w"),
		E("m", "Save a PNG of the active render into exportimgs/",
			"Guarda un PNG del render activo en exportimgs/", Scope::Nodes, "m"),
		E("d", "Toggle the compact debug overlay: FPS, box count and active composition",
			"Muestra u oculta el resumen de diagnóstico: FPS, cantidad de cajas y composición activa", Scope::Nodes, "d"),
		E("Single click", "Select a box and open its inspector",
			"Selecciona una caja y abre su inspector", Scope::Nodes),
		E("Double-click", "Make the box the active render",
			"Convierte la caja en el render activo", Scope::Nodes),
		E("Outlet drag", "Connect a box outlet to another box inlet",
			"Conecta la SALIDA de una caja con la ENTRADA de otra", Scope::Nodes),
		E("Empty drag", "Draw a rectangle to select several boxes",
			"Dibuja un rectángulo para seleccionar varias cajas", Scope::Nodes),
		E("Shift + drag",
			"Draw another rectangle and ADD what it touches to the current "
			"selection instead of replacing it",
			"Dibuja otro rectángulo y SUMA lo que toca a la selección actual en lugar de reemplazarla", Scope::Nodes, "shift"),
		E("Ctrl/Cmd + click",
			"Add or remove one box from the selection, leaving the rest alone",
			"Añade o quita una caja de la selección sin modificar las demás",
			Scope::Nodes, "ctrl"),
		E("Middle/right drag", "Pan the node canvas",
			"Desplaza el lienzo de nodos", Scope::Nodes),
		E("Mouse wheel", "Zoom the node canvas around the pointer",
			"Aplica zoom al lienzo alrededor del puntero", Scope::Nodes),
		GAP(),

		// ------------------------------------------------------------------
		H("ADDING BOXES", "AGREGAR CAJAS"),
		N("Also NODES only. IMPORT (4) is the browsable way to add shaders.",
			"Estos atajos también funcionan solo en NODES. IMPORT (4) permite explorar y agregar shaders.", Scope::Nodes),
		E("c", "Camera input", "Entrada Camera", Scope::Nodes, "c"),
		E("i", "Frame difference", "Diferencia entre cuadros", Scope::Nodes, "i"),
		E("b", "Paint canvas: draw by hand, animate cel by cel",
			"Lienzo Paint para dibujar y animar cuadro por cuadro",
			Scope::Nodes, "b"),
#ifdef NDI
		E("n", "NDI receiver", "Receptor NDI", Scope::Nodes, "n"),
#endif
#ifdef SPOUT
		E("h", "Spout input", "Entrada Spout", Scope::Nodes, "h"),
#endif
		E("Shift+C", "Kinect V2 input", "Entrada Kinect V2", Scope::Nodes, "shift+c"),
		E("Shift+D", "Camera depth: pseudo-depth from an ordinary camera",
			"Camera Depth: genera pseudoprofundidad desde una cámara convencional",
			Scope::Nodes, "shift+d"),
		E("Shift+P", "PointerCloud: Kinect V2 point cloud",
			"PointerCloud: nube de puntos del Kinect V2", Scope::Nodes, "shift+p"),
		GAP(),

		// ------------------------------------------------------------------
		H("PAINT CANVAS EDITOR", "EDITOR DEL LIENZO DE DIBUJO"),
		N("Add a paint box with b, select it, then press PAINT in the inspector header. These keys are live only while that panel is open.",
			"Agrega una caja paint con b, seleccionala y presiona PAINT en el encabezado del inspector. Estas teclas andan solo con ese panel abierto.",
			Scope::Paint),
		E("b / e", "Brush / eraser", "Pincel / goma", Scope::Paint, "b e"),
		E("y", "Cycles drawing symmetry: off, mirrored across the vertical axis, the horizontal one, or both. Every stroke commits its mirrors as real strokes",
			"Cambia la simetria de dibujo: apagada, espejada en el eje vertical, en el horizontal o en los dos. Cada trazo confirma sus espejos como trazos reales",
			Scope::Paint, "y"),
		E("l / r / o", "Line / rectangle / ellipse",
			"Linea / rectangulo / elipse", Scope::Paint, "l r o"),
		E("p", "Pen: draw an open sweep and it closes to its start point and fills on release",
			"Pluma: dibuja un trazo abierto y al soltar se cierra con el punto inicial y se rellena",
			Scope::Paint, "p"),
		E("g", "Fill: click a region to flood it. The size slider becomes the tolerance while this tool is selected",
			"Relleno: clic en una zona para rellenarla. El deslizador de tamano pasa a ser la tolerancia con esta herramienta",
			Scope::Paint, "g"),
		E("s / m", "Free / rectangular selection: clips without deforming; Shift adds and Alt subtracts. Drag to move, use the top handle to rotate or corners to scale. DEL removes it and D duplicates it",
			"Seleccion libre / rectangular: recorta sin deformar; Shift suma y Alt resta. Arrastra para mover, usa la manija superior para rotar o las esquinas para escalar. DEL borra y D duplica",
			Scope::Paint, "s m"),
		E("Ctrl/Cmd+A", "Select the whole active layer. Enter confirms and Esc deselects",
			"Seleccionar toda la capa activa. Enter confirma y Esc deselecciona",
			Scope::Paint, "ctrl+a"),
		E("Ctrl/Cmd+C / X / V", "Copy, cut and paste the selection, including between layers and cels",
			"Copiar, cortar y pegar la seleccion, incluso entre capas y cuadros",
			Scope::Paint, "ctrl c x v"),
		E("Arrows / Shift", "Move a selection by 1 or 10 pixels. Shift also constrains dragging, snaps rotation to 15 degrees and scale to 10%",
			"Mover la seleccion 1 o 10 pixeles. Shift tambien restringe el arrastre y ajusta la rotacion a 15 grados y la escala al 10%",
			Scope::Paint, "shift up down left right"),
		E("H / Shift+H", "Flip the selection horizontally / vertically",
			"Voltear la seleccion horizontal / verticalmente", Scope::Paint, "h shift"),
		E("Alt+right click", "Eyedropper: samples the colour under the cursor without leaving the tool you are holding",
			"Cuentagotas: toma el color bajo el cursor sin salir de la herramienta que tenes en la mano",
			Scope::Paint, "alt"),
		E("Hex", "The field at the bottom of the colour picker takes #RGB, #RRGGBB or #RRGGBBAA. Enter applies, Esc cancels, and it shows the current colour when not being edited",
			"El campo al pie del selector acepta #RGB, #RRGGBB o #RRGGBBAA. Enter aplica, Esc cancela, y muestra el color actual cuando no lo estas editando",
			Scope::Paint),
		E("[ / ]", "Brush size down / up",
			"Reducir / aumentar el tamano del pincel", Scope::Paint, "[ ]"),
		E("Brush property", "Click the SIZE label to choose size, opacity, hardness or stabilizer. Stylus/touch pressure controls point width when the backend provides it",
			"Clic en la etiqueta TAM para elegir tamano, opacidad, dureza o estabilizador. La presion de lapiz/touch controla el ancho cuando el sistema la informa",
			Scope::Paint),
		E("Ctrl/Cmd+Z", "Undo. Add Shift to redo",
			"Deshacer. Con Shift, rehacer", Scope::Paint, "ctrl+z"),
		E("Ctrl/Cmd+J / Ctrl/Cmd+E", "Duplicate the active layer / merge it down (normal, opaque, visible, unlocked layers)",
			"Duplicar la capa activa / combinarla hacia abajo (capas normales, opacas, visibles y desbloqueadas)", Scope::Paint, "ctrl j e"),
		E("Layer N/M/S/+", "Cycle Normal, Multiply, Screen and Add blend modes. The lock prevents drawing and destructive edits",
			"Alternar mezcla Normal, Multiplicar, Trama y Sumar. El candado impide dibujar y editar destructivamente", Scope::Paint),
		E("Ctrl/Cmd+Shift+P", "Export the current cel as PNG",
			"Exportar el cuadro actual como PNG", Scope::Paint, "ctrl+shift+p"),
		E("Ctrl/Cmd+Alt+P", "Export every cel as a numbered PNG sequence",
			"Exportar todos los cuadros como secuencia PNG numerada", Scope::Paint, "ctrl+alt+p"),
		E("Ctrl/Cmd+Shift+G", "Export an animated GIF using FPS and each cel's hold duration",
			"Exportar GIF animado usando los FPS y la duracion de cada cuadro", Scope::Paint, "ctrl+shift+g"),
		E(", / .", "Previous / next cel", "Cuadro anterior / siguiente",
			Scope::Paint, ", ."),
		E("Space", "Play or pause the animation",
			"Reproducir o pausar la animacion", Scope::Paint, "space"),
		E("Direction", "The arrow in the transport row plays forwards or backwards. Ping-pong flips it as it bounces; changing the playback mode restores the direction you chose",
			"La flecha en la fila de transporte reproduce hacia adelante o atras. Ping-pong la invierte al rebotar; al cambiar el modo se restaura la direccion que elegiste",
			Scope::Paint),
		E("n / d", "New cel / duplicate the current cel",
			"Cuadro nuevo / duplicar el cuadro actual", Scope::Paint, "n d"),
		E("Del", "Delete the current cel, never the box",
			"Borrar el cuadro actual, nunca la caja", Scope::Paint, "del"),
		E("- / =", "Shorten or lengthen how long this cel is held",
			"Acortar o alargar cuanto dura este cuadro", Scope::Paint, "- ="),
		E("Shift+O", "Cycle the onion skin range",
			"Cambiar el rango del papel cebolla", Scope::Paint, "shift+o"),
		E("< / >", "Select the layer below / above",
			"Seleccionar la capa de abajo / de arriba", Scope::Paint, "shift , ."),
		E("Double click", "On a layer's name, rename it. Enter applies, Esc cancels",
			"Sobre el nombre de una capa, renombrarla. Enter aplica, Esc cancela",
			Scope::Paint),
		E("Timeline", "Rows are layers, columns are frames, like Aseprite. Click a cell to select that frame AND that layer; a filled marker means the layer has strokes there. All layers always share the same frames",
			"Las filas son capas y las columnas cuadros, como en Aseprite. Clic en una celda selecciona ese cuadro Y esa capa; un marcador lleno indica que la capa tiene trazos ahi. Todas las capas comparten siempre los mismos cuadros",
			Scope::Paint),
		E("Timeline blocks", "Click selects one cell. Shift+click selects a rectangular range; Ctrl+click (Windows/Linux) or Cmd+click (macOS) toggles individual cells. Drag a selected block to move it to empty cells. Ctrl/Cmd+C, X and V copy, cut and paste; D duplicates and Del/Backspace clears the block. Each block operation is one Undo step",
			"Bloques de la linea de tiempo: clic selecciona una celda. Shift+clic selecciona un rango rectangular; Ctrl+clic (Windows/Linux) o Cmd+clic (macOS) alterna celdas individuales. Arrastra un bloque seleccionado para moverlo solo a celdas vacias. Ctrl/Cmd+C, X y V copian, cortan y pegan; D duplica y Del/Backspace limpia el bloque. Cada operacion es un solo paso de Undo",
			Scope::Paint),
		E("Drag", "A frame number reorders frames, a layer row reorders layers. The wheel scrolls frames over the grid and layers over the left gutter",
			"Un numero de cuadro reordena cuadros, una fila de capa reordena capas. La rueda desplaza cuadros sobre la grilla y capas sobre la columna izquierda",
			Scope::Paint),
		E("BG", "Marks a layer as the background: it is drawn from one shared set of strokes on EVERY frame, so a static backdrop is drawn once instead of copied onto each frame. Its row shows as a single band rather than per-frame markers",
			"Marca una capa como fondo: se dibuja desde un solo conjunto de trazos en TODOS los cuadros, asi un fondo fijo se dibuja una vez en lugar de copiarse en cada cuadro",
			Scope::Paint),
		E("Ctrl/Cmd+Alt+G", "Export every frame as one sprite sheet, laid out in a roughly square grid",
			"Exportar todos los cuadros en una hoja de sprites, en una grilla casi cuadrada",
			Scope::Paint, "ctrl+alt+g"),
		// Was E("?"), which documented a key nothing handles: paintHelpOpen is
		// only ever set by the header icon.
		E("? icon", "This list. Esc closes it, Esc again closes the panel",
			"Esta lista. Esc la cierra, Esc otra vez cierra el panel", Scope::Paint),
		E("DEL / BACKSPACE", "Clears the current cel, or deletes selected strokes if a selection is active",
			"Limpia la celda actual, o borra los trazos seleccionados si hay una seleccion activa",
			Scope::Paint, "del backspace"),
		E("Shift + DEL", "Deletes the current frame from the timeline",
			"Borra el cuadro actual de la linea de tiempo",
			Scope::Paint, "shift+del"),
		E("Right click", "On a palette swatch, removes it from the palette",
			"Sobre un color de la paleta, lo quita",
			Scope::Paint),

		E("ONION", "A click keeps both sides of the onion range in step. Shift+click changes only the frames BEFORE, Alt+click only the ones after",
			"Un clic mueve los dos lados del rango de cebolla juntos. Shift+clic cambia solo los cuadros ANTERIORES y Alt+clic solo los siguientes",
			Scope::Paint),
		E("Bucket", "A fill stores the region it covered, not the click that made it - so it can be selected, moved and transformed like any other mark, and it does not change when something under it is edited",
			"El balde guarda la region que cubrio, no el clic que lo hizo: se puede seleccionar, mover y transformar como cualquier otra marca, y no cambia si se edita algo debajo",
			Scope::Paint),
		E("Export", "The page icon in the panel header opens the export options: a resolution multiplier, whether to export the whole document or only the transport's IN/OUT range, and the sprite sheet",
			"El icono de hoja en el encabezado abre las opciones de exportacion: multiplicador de resolucion, exportar todo el documento o solo el rango IN/OUT del transporte, y la hoja de sprites",
			Scope::Paint),
		E("Palette", "Click the colour swatch to open the picker, then + to save the current colour. The palette is kept in data/paint_palette.xml and survives restarts",
			"Clic en el color abre el selector, y + guarda el color actual. La paleta se guarda en data/paint_palette.xml y sobrevive reinicios",
			Scope::Paint),
		E("Ctrl+drag", "Pan the canvas. Middle drag does the same, scroll to zoom",
			"Mover el lienzo. El boton del medio hace lo mismo, rueda para acercar",
			Scope::Paint, "ctrl"),
		N("The texture input is a tracing reference: it shows in the editor and is never part of what the box outputs.",
			"La entrada de textura es una referencia para calcar: se ve en el editor y nunca forma parte de lo que la caja saca.",
			Scope::Paint),
		GAP(),

		// ------------------------------------------------------------------
		H("IMPORT: SHADER BROWSER", "IMPORT: NAVEGADOR DE SHADERS"),
		N("Press 4 or select IMPORT. Search receives focus immediately; press ESC before using letter shortcuts such as h.",
			"Pulsa 4 o selecciona IMPORT. La búsqueda recibe el foco de inmediato; pulsa ESC antes de usar atajos de letras como h.", Scope::Import),
		E("Type", "Filter shaders by name while the search field is focused",
			"Filtra shaders por nombre mientras el campo de búsqueda está enfocado", Scope::Import),
		E("Up / Down", "Move through the filtered results",
			"Recorre los resultados filtrados", Scope::Import, "up down"),
		E("Enter", "Load the selected shader as a box; double-clicking a result does the same",
			"Carga el shader seleccionado como caja; un doble clic sobre el resultado hace lo mismo", Scope::Import, "enter"),
		E("Star icon", "Add or remove a favourite. Favourites persist in data/shader_favorites.xml",
			"Añade o quita un favorito. Los favoritos se guardan en data/shader_favorites.xml", Scope::Import),
		E("BIND / MOVE MIDI", "Learn a MIDI control that adds this shader without opening the MIDI screen",
			"Aprende un control MIDI que agrega este shader sin abrir la pantalla MIDI", Scope::Import),
		E("h", "Debug aid: outlines the clickable area of every shader in the list. Press ESC first, or the search field takes the key",
			"Ayuda de diagnóstico: delimita el área interactiva de cada shader. Pulsa ESC primero o la búsqueda recibirá la tecla",
			Scope::Import, "h"),
		GAP(),

		// ------------------------------------------------------------------
		H("SHADER EDITOR", "EDITOR DE SHADERS"),
		N("Press 5 or the EDITOR tab to edit the selected shader. Open files appear as tabs.",
			"Pulsa 5 o la pestaña EDITOR para editar el shader seleccionado. Cada archivo abierto aparece en una pestaña.", Scope::Editor),
		E("Ctrl/Cmd+S", "Save the shader file. This does not save the composition",
			"Guarda el archivo del shader. Esta acción no guarda la composición", Scope::Editor, "ctrl+s"),
		E("Ctrl/Cmd+C / X / V / A", "Copy, cut, paste and select all inside the editor",
			"Copia, corta, pega y selecciona todo dentro del editor", Scope::Editor, "ctrl c x v a"),
		E("Home / End", "Start and end of the line. PageUp and PageDown scroll",
			"Va al inicio o final de la línea. PageUp y PageDown desplazan el documento", Scope::Editor, "home end"),
		E("Shift + arrows", "Select text",
			"Selecciona texto", Scope::Editor, "shift up down left right"),
		// A cross-reference, not a second key row: r is already documented under
		// NODE GRAPH, and a duplicate E() would claim the same cap twice.
		N("Save here, then go back to NODES and press r to reload the shader and see your changes.",
			"Guarda aquí, regresa a NODES y pulsa r para recargar el shader y ver los cambios.",
			Scope::Nodes),
		GAP(),

		// ------------------------------------------------------------------
		H("MIDI MAPPING", "MAPEO MIDI"),
		N("Press 6 or select MIDI. Bindings include the device identity, so different controllers keep independent maps.",
			"Pulsa 6 o selecciona MIDI. Las asignaciones incluyen la identidad del dispositivo, por lo que distintos controladores conservan mapas independientes.", Scope::Midi),
		E("Key bind map on/off",
			"Enable or disable direct mapping mode. An indicator appears in the top bar while it is active",
			"Activa o desactiva el modo de asignación directa. Mientras está activo aparece un indicador en la barra superior", Scope::Midi),
		E("Learn",
			"Arm a row, then move or press a MIDI control to bind it",
			"Prepara una fila y luego mueve o pulsa un control MIDI para asignarlo", Scope::Midi),
		N("If that control is already bound you get a prompt: Replace, Keep both, or Cancel. Keep both fires every binding on the key.",
			"Si el control ya está asignado, elige Replace, Keep both o Cancel. Keep both ejecuta todas las acciones asociadas a ese control.", Scope::Midi),
		E("Target box + Action",
			"Create a custom binding by choosing a target box, an action and, for Parameter, a parameter index",
			"Crea una asignación personalizada eligiendo una caja, una acción y, para Parameter, un índice de parámetro", Scope::Midi),
		E("Rescan devices",
			"Detect controllers connected after startup",
			"Detecta controladores conectados después de iniciar", Scope::Midi),
		N("Global actions include previous/next box, cue, active render, previous/next gallery item, gallery toggle and BPM tap. Gallery can also be toggled with e or OSC.",
			"Las acciones globales incluyen caja anterior/siguiente, cue, render activo, elemento anterior/siguiente de galería, activación de galería y BPM tap. La galería también puede activarse con e u OSC.", Scope::Midi),
		N("With mapping mode on you can also click a box button or an inspector slider directly to arm it.",
			"Con el modo de asignación activo, también puedes hacer clic directamente en un botón de caja o slider del inspector para prepararlo.", Scope::Midi),
		N("For a parameter following Audio, MIDI controls Amount from 0 to 1. Other automated parameters keep mapping MIDI to automation speed.",
			"Si un parámetro sigue Audio, MIDI controla Amount entre 0 y 1. Los demás parámetros automatizados mantienen el control MIDI sobre la velocidad.", Scope::Midi),
		GAP(),

		// ------------------------------------------------------------------
		H("CUE AND PROJECTION MAPPING", "CUE Y MAPPING DE PROYECCIÓN"),
		N("Cue stages a box for off-air adjustment. The panel displays the staged result in amber while the live output continues unchanged.",
			"CUE prepara una caja para ajustarla fuera del aire. El panel muestra el resultado preparado en ámbar mientras la salida en vivo continúa sin cambios."),
		E("CUE / z", "Toggle cue for the selected box; with no selection, use the active render. Repeating the action clears cue",
			"Activa o desactiva el cue de la caja seleccionada; si no hay selección, usa el render activo. Repetir la acción limpia el cue", Scope::Global, "z"),
		E("MAP", "Open or close mapping for the selected mapping shader. The button stays disabled when the selection does not support mapping",
			"Abre o cierra el mapping del shader de mapping seleccionado. El botón permanece deshabilitado si la selección no admite mapping"),
		E("IMG FINAL", "Open the box-less image stack drawn above the active render and its transitions. Use + or drop several PNG, JPG or GIF files on the preview",
			"Abre la pila de imágenes sin cajas que se dibuja sobre el render activo y sus transiciones. Usa + o suelta varios PNG, JPG o GIF sobre el preview"),
		E("IMG MAP", "Open the image stack inside the selected Mapping Advance layer. Its connected texture remains the background; the images are composed before warp and mask",
			"Abre la pila de imágenes dentro de la capa seleccionada de Mapping Advance. La textura conectada queda como fondo; las imágenes se componen antes del warp y la máscara"),
		E("Image drag", "Click the top image and drag to move it. Alt-click cycles through overlapping images; side and corner handles scale, Shift preserves aspect, and the round handle rotates with 15-degree Shift snapping",
			"Haz clic en la imagen superior y arrastra para moverla. Alt+clic recorre imágenes superpuestas; los controles laterales y de esquina escalan, Shift conserva la proporción y el control redondo rota con ajuste de 15 grados usando Shift"),
		E("Image stack", "Toggle visibility, drag opacity, reorder or delete rows. GIF rows also provide play/pause, restart and speed. Ctrl/Cmd+Z undoes the open image editor",
			"Alterna visibilidad, arrastra opacidad, reordena o borra filas. Los GIF también ofrecen play/pausa, reinicio y velocidad. Ctrl/Cmd+Z deshace el editor de imágenes abierto"),
		N("In the mapping editor, drag corners to fit the projection surface and use the visibility controls for borders, points and the curved grid.",
			"En el editor de mapping, arrastra las esquinas para ajustar la superficie de proyección y usa los controles de visibilidad para bordes, puntos y grilla curva."),
		E("Mask pen", "Click to add points, click the first point to close, click a closed edge to insert a point, and right-click a point to delete it",
			"Haz clic para agregar puntos, pulsa el primero para cerrar, pulsa un borde cerrado para insertar un punto y haz clic derecho sobre un punto para borrarlo"),
		E("Ellipse / Circle", "Drag from the centre to create an ellipse; hold Shift for a circle measured in output pixels",
			"Arrastra desde el centro para crear una elipse; mantén Shift para un círculo medido en píxeles de salida"),
		E("New Shape (+)", "After closing the selected mask, start another independent mask shape on the same texture layer",
			"Después de cerrar la máscara seleccionada, inicia otra forma independiente en la misma capa de textura"),
		E("MOVE target icons", "The highlighted Mesh or Pen icon shows whether MOVE targets the surface or masks. Click either icon while moving to switch target",
			"El icono Mesh o Pen resaltado indica si MOVE actúa sobre la superficie o las máscaras. Pulsa cualquiera para cambiar el objetivo"),
		E("Mask selection", "With MOVE targeting Pen, click a mask to select it, Shift-click to toggle it, or drag empty space for a marquee; Shift-marquee adds masks",
			"Con MOVE apuntando a Pen, haz clic en una máscara para seleccionarla, Shift+clic para alternarla o arrastra en el vacío para crear un marco; Shift+marco suma máscaras"),
		E("Mask transform", "Drag inside a selected mask to move the group, use corners to scale uniformly, or the curved handle to rotate; Shift snaps rotation to 15 degrees",
			"Arrastra dentro de una máscara seleccionada para mover el grupo, usa las esquinas para escalar proporcionalmente o el control curvo para rotar; Shift ajusta la rotación a 15 grados"),
		E("Union / A-B", "Select A, then Shift-click B. Union combines them; A-B cuts B from A. Results can be chained with another independent shape",
			"Selecciona A y luego usa Shift+clic en B. Unión los combina; A-B recorta B de A. El resultado puede encadenarse con otra forma independiente"),
		E("Boolean components", "Click a boolean result to move it as one shape; Alt-click a component to edit or transform it without breaking the operation",
			"Haz clic en un resultado booleano para moverlo completo; usa Alt+clic en un componente para editarlo o transformarlo sin romper la operación"),
		E("Mapping wheel", "Zoom the advanced-mapping preview around the pointer from 100% to 1600%",
			"Aplica zoom al preview de mapping avanzado alrededor del puntero, entre 100% y 1600%"),
		E("Right drag / Middle drag", "Pan the mapping preview in any tool without changing geometry. A stationary right-click on a Pen point deletes it",
			"Desplaza el preview desde cualquier herramienta sin modificar la geometría. Un clic derecho sin arrastre sobre un punto Pen lo borra"),
		E("Zoom %", "Click the percentage in the mapping header to fit and center the preview",
			"Pulsa el porcentaje del encabezado para volver a 100% y centrar el preview"),
		GAP(),

		// ------------------------------------------------------------------
		H("SETTINGS AND OUTPUT", "CONFIGURACIÓN Y SALIDA"),
		N("Press 2 or select SETTINGS. Configure OSC ports and destination, render size, BPM/AUTOTAP, Spout and NDI, default composition, Audio and live outputs.",
			"Pulsa 2 o selecciona SETTINGS. Configura puertos y destino OSC, tamaño de render, BPM/AUTOTAP, Spout y NDI, composición predeterminada, Audio y salidas en vivo.", Scope::Settings),
		E("Live outputs",
			"Add one output per screen, then choose its monitor, source and crop",
			"Agrega una salida por pantalla y elige su monitor, fuente y recorte", Scope::Settings),
		E("Fullscreen",
			"Enable full-screen mode for that output. This replaces the former global f shortcut",
			"Activa el modo de pantalla completa para esa salida. Reemplaza al antiguo atajo global f", Scope::Settings),
		E("Screen wall",
			"Arrange outputs as free crops, a grid, or physical positions and sizes measured in millimetres",
			"Distribuye las salidas mediante recortes libres, una grilla o posiciones y tamaños físicos medidos en milímetros", Scope::Settings),
		E("Tab / Shift+Tab", "Move between fields while editing a live output or wall split",
			"Avanza o retrocede entre campos al editar una salida o división del muro", Scope::Settings, "tab shift"),
		GAP(),

		// ------------------------------------------------------------------
		H("OSC", "OSC"),
		N("Send the address shown below with one numeric argument. Commands that ignore its value still require the argument.",
			"Envía la dirección indicada con un argumento numérico. Los comandos que ignoran su valor también requieren ese argumento."),
		E("/load/<file>", "Load savefiles/<file>; the numeric argument is ignored",
			"Carga savefiles/<archivo>; el argumento numérico se ignora"),
		E("/setactiverender", "Argument: box index to make active in the current graph",
			"Argumento: índice de la caja que se convertirá en render activo del grafo actual"),
		E("/nextshader", "Select the next box",
			"Selecciona la caja siguiente"),
		E("/prevshader", "Select the previous box",
			"Selecciona la caja anterior"),
		E("/setactiveshader", "Make the currently selected box active",
			"Convierte la caja seleccionada en render activo"),
		E("/nextshader_gallerymode", "Select and activate the next box",
			"Selecciona y activa la caja siguiente"),
		E("/prevshader_gallerymode", "Select and activate the previous box",
			"Selecciona y activa la caja anterior"),
		E("/setactivecycle", "Toggle sequence/gallery mode",
			"Activa o desactiva el modo secuencia/galería; al activarlo enciende las cajas del nivel superior"),
		E("/disablegallerymode", "Disable gallery mode and enable every top-level box",
			"Desactiva el modo galería y enciende todas las cajas del nivel superior"),
		E("/setdurationgalleryms", "Argument: gallery duration in milliseconds",
			"Argumento: duración de la galería en milisegundos"),
		E("/addmirrorsquad", "Add the mirrorquad shader box",
			"Agrega una caja con el shader mirrorquad"),
		E("/<box>/<parameter>", "Set a float parameter on a top-level box, matching both names exactly. Use /<box>/onoff with exactly 0 or 1 for its power state",
			"Cambia un parámetro float de una caja del nivel superior, buscando ambos nombres de forma exacta. Usa /<caja>/onoff con 0 o 1 para su estado"),
		E("/openguinumber/<parameterIndex>", "Set a standard float parameter by bind-slot index in the open inspector; malformed or out-of-range indices are ignored",
			"Cambia por índice de binding un parámetro float estándar del inspector abierto; los índices inválidos o fuera de rango se ignoran"),
		GAP(),

		// ------------------------------------------------------------------
		H("GLOBAL SHADER UNIFORMS", "UNIFORMES GLOBALES DE SHADER"),
		N("Declare any of these in a .frag and Guipper updates it every rendered frame.",
			"Declara cualquiera de estos uniformes en un .frag y Guipper actualizará su valor en cada cuadro renderizado."),
		E("uniform float time;", "Seconds elapsed since Guipper started",
			"Segundos transcurridos desde que se inició Guipper"),
		E("uniform vec2 resolution;", "This box's render resolution in pixels",
			"Resolución del render de esta caja, en píxeles"),
		E("uniform float bpm;", "Global BPM shared by AUTOTAP, BPM tap and BPM automation",
			"BPM global compartido con AUTOTAP, BPM tap y la automatización BPM"),
		E("uniform vec4 mouse;", "Normalized main-window pointer: xy is current and zw stores the last press",
			"Puntero normalizado de la ventana principal: xy es la posición actual y zw conserva la última pulsación"),
		E("uniform vec2 window_mouse;", "Normalized pointer in the separate render window. Display-only outputs do not update it",
			"Puntero normalizado de la ventana de render independiente. Las salidas de visualización no modifican este valor"),
		E("uniform int globalframeNum;", "Frames elapsed since Guipper started",
			"Cantidad de cuadros transcurridos desde que se inició Guipper"),
		E("uniform int boxframeNum;", "Frames processed since this box started or reloaded",
			"Cantidad de cuadros procesados desde que se inició o recargó esta caja"),
		E("uniform sampler2D feedback;", "The previous rendered frame of this box, stored in a separate texture",
			"Cuadro renderizado anterior de esta caja, almacenado en una textura separada"),
		GAP(),

		// ------------------------------------------------------------------
		H("AUDIO", "AUDIO"),
		N("In SETTINGS, enable AUDIO IN, select a device and adjust Gain. The meter shows spectrum, kick/snare detection, estimated BPM, confidence and clipping.",
			"En SETTINGS, activa AUDIO IN, elige un dispositivo y ajusta Gain. El medidor muestra el espectro, detecciones de bombo y redoblante, BPM estimado, confianza y clipping.", Scope::Settings),
		N("MIX combines stereo channels; LEFT and RIGHT listen to one channel. AUTO GAIN adapts analysis to the incoming level. CALIBRATE listens for three seconds and sets the noise threshold.",
			"MIX combina los canales estéreo; LEFT y RIGHT escuchan uno solo. AUTO GAIN adapta el análisis al nivel recibido. CALIBRATE escucha durante tres segundos y ajusta el umbral al ruido ambiente.", Scope::Settings),
		N("The Audio button makes any float parameter follow the analyser. The source chip selects its signal; a custom slider range limits the movement.",
			"El botón de Audio hace que cualquier parámetro float siga el análisis. El chip de fuente elige la señal; el rango personalizado del slider limita el recorrido."),
		N("Open Shaping to adjust Amount, Normal/Inverted polarity, Threshold, Curve, Attack and Release. Times are milliseconds and remain independent of frame rate.",
			"Abre Shaping para ajustar Amount, polaridad Normal/Inverted, Threshold, Curve, Attack y Release. Los tiempos están expresados en milisegundos y no dependen de los FPS."),
		E("Low / Mid / High", "Normalized energy in the low, middle and high frequency bands",
			"Energía normalizada de las bandas graves, medias y agudas"),
		E("Kick / Snare", "Short envelopes generated by detected kick and snare onsets",
			"Envolventes breves generadas al detectar un golpe de bombo o redoblante"),
		E("Low bass / High mid", "Low+kick and high+snare blends for a steadier response",
			"Combinan graves con bombo y agudos con redoblante para producir una respuesta más estable"),
		E("Level", "Normalized overall input level", "Nivel general normalizado de la entrada"),
		E("Kick trigger / Snare trigger", "A short pulse every 1, 2, 4, 8 or 16 detected onsets",
			"Pulso breve cada 1, 2, 4, 8 o 16 detecciones de la fuente elegida"),
		E("Kick envelope / Snare envelope", "Hold the detected intensity until the next counted onset",
			"Conserva la intensidad detectada hasta la siguiente detección contada"),
		E("Kick logic / Snare logic", "Toggle between 0 and 1 on every counted onset",
			"Alterna entre 0 y 1 en cada detección contada"),
		N("For trigger, envelope and logic sources, the Every chip selects how often they act: every 1, 2, 4, 8 or 16 detections.",
			"Para trigger, envelope y logic, el chip Every selecciona cada cuántas detecciones actúan: 1, 2, 4, 8 o 16."),
		N("If the slider has a MIDI binding while following Audio, MIDI controls Amount: 0 removes modulation and 1 applies the full range.",
			"Si el slider tiene una asignación MIDI mientras sigue Audio, MIDI controla Amount: 0 elimina la modulación y 1 aplica todo el recorrido."),
		E("uniform vec4 audio_bands;", "x low, y mid, z high, w level",
			"x grave, y medio, z agudo, w nivel"),
		E("uniform vec4 audio_hits;", "x kick, y snare, z low+kick, w high+snare",
			"x bombo, y redoblante, z grave+bombo, w agudo+redoblante"),
		E("uniform float audio_trigger;", "Kick pulse using the global division selected in SETTINGS",
			"Pulso de bombo según la división global elegida en SETTINGS"),
		E("uniform float audio_express;", "Kick intensity held until the next counted detection",
			"Intensidad del bombo conservada hasta la siguiente detección contada"),
		E("uniform float audio_logic;", "0/1 state toggled by every counted kick detection",
			"Estado 0/1 que alterna con cada detección de bombo contada"),
		E("uniform vec4 audio_onsets;", "x kick trigger, y snare trigger, z kick logic, w snare logic",
			"x trigger de bombo, y trigger de redoblante, z lógica de bombo, w lógica de redoblante"),
		E("uniform vec4 audio_rhythm;", "x beat phase 0..1, y beat pulse 0/1, z detected BPM, w confidence 0..1",
			"x fase 0..1, y pulso 0/1, z BPM detectado, w confianza 0..1"),
		E("uniform vec4 audio_spectrum0..3;", "Sixteen normalized log-frequency bins, four bins per vec4",
			"Dieciséis bandas logarítmicas normalizadas, cuatro en cada vec4"),
		};
		return t;
	}
}
