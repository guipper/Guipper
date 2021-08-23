#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{

	font_p.loadFont("font/Montserrat-Regular.ttf", 11); //Inicio fuente.

	ofSetVerticalSync(false);
	//FreeConsole();

	ofDisableArbTex();
	//ESTAS COSAS LAS SETEA DESDE EL SETTINGS XML DESPUES. PERO ACA LES DA UN VALOR INICIAL  POR LAS DUDAS :
	float altoventana = ofGetScreenHeight() * 3 / 4;
	float anchoventana = ofGetScreenWidth() * 3 / 4;
	ofSetWindowShape(anchoventana, altoventana);
	ofSetWindowPosition(ofGetScreenWidth() / 2 - anchoventana / 2,
						ofGetScreenHeight() / 2 - altoventana / 2);

	jp_constants::init(ofGetScreenWidth(), ofGetScreenHeight(), 600, 600);
	jp_constants::setwindow_mousex(300);
	jp_constants::setwindow_mousex(300);
	jp_constants::set_systemDialog_open(false);
	jp_constants_img::init();
	cout << "Render width  " << jp_constants::renderWidth << endl;
	boxes.setup(font_p, activerender);
	ofSetBackgroundColor(0);
	ofSetFrameRate(60);
	openloader.setJPboxgroupPointer(boxes);
	ofAddListener(ofGetWindowPtr()->events().keyPressed, this,
				  &ofApp::keycodePressed);

	savedirectory = "savefiles/data.xml";
	receiver.setup(PORT);
	oscout_mode1 = true;
	oscout_mode2 = true;
	setInitialValues();
	ofSetWindowTitle("GUIPPER");

	loadAspreset = false;
	//.----------------------------------------------------------------/

#ifdef NDI
	//INIT NDI :
	// Optionally set fbo readback using OpenGL pixel buffers
	ndiSender.SetReadback(); // Change to false to compare
	// Optionally set NDI asynchronous sending
	// instead of clocked at the specified frame rate (60fps default)
	ndiSender.SetAsync();
	//ndiSender.
	ndiSender.CreateSender(sendername, jp_constants::renderWidth, jp_constants::renderHeight);
//---------------------------------------------------/
#endif

	pantallaActiva = NODOS;

	//INIT SPOUT SENDER :
	ofBackground(10, 100, 140);
	ofEnableNormalizedTexCoords(); // explicitly normalize tex coords for ofBox

	strcpy(sendername, "Guipper"); // Set the sender name
	ofSetWindowTitle(sendername);  // show it on the title bar

// ====== SPOUT =====
#ifdef SPOUT
	bInitialized = false; // Spout sender initialization

	// Create an OpenGL texture for data transfers
	sendertexture = 0; // make sure the ID is zero for the first time
	//InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	//OK . Esto tira error si no pasas otros valores que no sean : ofGetWidth(), ofGetHeight().
	//HAbria que ver por que . . . . . Y si le tiras un resize la mata.
	//cout << "ANCHO VENTANA " << ofGetWidth() << endl;//1440
	//cout << "ALTO VENTANA " << ofGetHeight() << endl;//810
	//InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	//InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?

	resolution_spoutext = ofVec2f(ofGetWidth(), ofGetHeight());
	//resolution_spoutext = ofVec2f(ofGetWidth(),ofGetHeight());

	//LA RECONCHA DEL PATO, NO HAY MANERA POSIBLE PARA QUE PASE EL SPOUT EN UNA RESOLUCION
	//MAS GRANDE QUE EN LA VENTANA DEL ORTO!??!?!?!?!?!
	InitGLtexture(sendertexture, resolution_spoutext.x, resolution_spoutext.y); //!?!??!!?
#endif

	// 3D drawing setup for a sender
	//glEnable(GL_DEPTH_TEST);							// enable depth comparisons and update the depth buffer
	//glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
	ofDisableArbTex(); // needed for textures to work
	//myTextureImage.loadImage("SpoutBox1.png");			// Load a texture image for the demo
	boxes.load(savedirectory);
}

void ofApp::update()
{
	boxes.update();

#ifdef SPOUT
	if (spoutActive && boxes.getBoxesSize() > 0)
	{
		ofClear(0);
		drawSpout();
	}
#endif

#ifdef NDI
	if (boxes.getBoxesSize() > 0)
	{
		ndiSender.SendImage(*boxes.getActiverender());
	}
#endif

	updateOSC();

	if (saveas_saver.activeflag)
	{
		savedirectory = saveas_saver.path;
		boxes.save(savedirectory);
		saveas_saver.activeflag = false;
	}

	//PARA MANEJAR UN PROCESO PARALELO QUE CARGA LOS ARCHIVOS.
	/*if (openloader.activeflag) {
		try
		{	
			boxes.addBox(openloader.path);
			//if (openloader.activeFiletype == openloader.SHADER) {
				//boxes.addShaderBox(openloader.path);
		//	}
			//else if (openloader.activeFiletype == openloader.SAVEFILE) {
				//savedirectory = openloader.path;
				//boxes.load(savedirectory);
			//}
			//else if (openloader.activeFiletype == openloader.IMAGE) {
				//boxes.addImageBox(openloader.path);
			//}
			//else if (openloader.activeFiletype == openloader.VIDEO) {
				//boxes.addVideoBox(openloader.path);
			//}
		//}
		catch (const std::exception&)
		{
			cout << "COULD NOT OPEN FILE" << endl;
		}
		
		openloader.activeflag = false;
	}*/
}

void ofApp::draw()
{
	boxes.draw_activerender(ofGetWidth(), ofGetHeight());
	if (pantallaActiva == NODOS)
	{
		boxes.draw();
		ofSetColor(255, 255, 255, 255);
		//outletimg.draw(ofGetWidth() / 2, ofGetHeight() / 2, 200, 200);
		if (isDebug)
		{
			draw_debugInfo();
		}
	}
	if (pantallaActiva == TUTORIAL)
	{
		draw_instrucciones();
	}
	if (pantallaActiva == OPCIONES)
	{
		draw_opciones();
	}
}

void ofApp::draw_debugInfo()
{

	float sepy = 20;
	float posy = ofGetHeight();
	font_p.drawString("FRAMERATE :" + ofToString(ofGetFrameRate()), 30, posy -= sepy);
	font_p.drawString("Boxes size : " + ofToString(boxes.getBoxesSize()), 30, posy -= sepy);
	font_p.drawString("DIALOG BOX : " + ofToString(jp_constants::systemDialog_open), 30, posy -= sepy);
}

void ofApp::draw_instrucciones()
{
	float x = 100;
	float y = 100;
	float sepy = 30;

	ofSetColor(255);
	jp_constants::p_font.drawString("t : Cambia ente cargar un xml como preset o como compo", x, y += sepy);
	jp_constants::p_font.drawString("w : Abre ventana aparte", x, y += sepy);
	jp_constants::p_font.drawString("f sobre la ventana aparte abierta : FullScreen", x, y += sepy);
	jp_constants::p_font.drawString("s : guardar sobre el xml actual : " + savedirectory, x, y += sepy);
	jp_constants::p_font.drawString("crtl+s : Genera un nuevo archivo XML(guardarlo con la extension correcta)", x, y += sepy);
	jp_constants::p_font.drawString("d: muestra los datos de debug", x, y += sepy);
	jp_constants::p_font.drawString("r: recarga el shader activo", x, y += sepy);
	jp_constants::p_font.drawString("h : agrega caja SPOUT INPUT", x, y += sepy);
	jp_constants::p_font.drawString("c : agrega caja camara", x, y += sepy);
	jp_constants::p_font.drawString("m: exportar imagen", x, y += sepy);
	jp_constants::p_font.drawString("Para cargar archivo (todos) arrastrarlo hasta la ventana", x, y += sepy);
}

void ofApp::draw_opciones()
{

	float sepy = 20;
	float posy = 30;
	float posx = 30;

	font_p.drawString("opciones de SETTINGS.XML", posx, posy += sepy);
	font_p.drawString("Render Resolution :" + ofToString(jp_constants::renderWidth) + "X" + ofToString(jp_constants::renderHeight), posx, posy += sepy);
#ifdef SPOUT
	font_p.drawString("Spout " + ofToString(spoutActive), posx, posy += sepy);
#endif
	font_p.drawString("osc PORT IN " + ofToString(receiver.getPort()), posx, posy += sepy);
	font_p.drawString("osc PORT OUT " + ofToString(sender.getPort()), posx, posy += sepy);
	font_p.drawString("osc IP OUT " + ofToString(sender.getHost()), posx, posy += sepy);
}

//Esta es la que se dibuja en la otra ventana
void ofApp::drawRender()
{
	boxes.draw_activerender();
}

void ofApp::keyPressed(int key)
{

	//keyIsDown[key] = true;

	if (key == '1')
	{
		pantallaActiva = NODOS;
	}

	if (key == '2')
	{
		pantallaActiva = OPCIONES;
	}

	if (key == '3')
	{
		pantallaActiva = TUTORIAL;
	}

	if (pantallaActiva == NODOS)
	{
		if (key == 't')
		{
			loadAspreset = !loadAspreset;
		}
		if (key == 'f')
		{
			//ofToggleFullscreen();
		}
		if (key == 'h')
		{
			boxes.addBox("spoutReceiver");
		}
		if (key == 'i')
		{
			boxes.addBox("framedifference");
		}
		if (key == 'c')
		{
			//boxes.addCamBox();
			boxes.addBox("cam");
		}
		if (key == 'n')
		{
			boxes.addBox("ndiReceiver");
		}
		if (key == OF_KEY_DEL)
		{
			cout << "DEL " << endl;
			boxes.deleteSelectedShader();
		}

		if (key == 'o')
		{
			//openloader.startThread();
			//ESTO ES LO QUE HABRIA QUE PROBAR EN MAC PARA VER SI FUNCA O NO . CUANDO ESTEMOS AH�
			/*ofFileDialogResult result = ofSystemLoadDialog("Load file");
			if (result.bSuccess) {
				string path = result.getPath();
				cout << "path " << path << endl;
				if (path.find("data") != std::string::npos) {
					cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
					path = path.substr(path.find("data"), path.size());
					cout << "NEW PATH CONVERSION :" << path << endl;
				}
				else {
					cout << "WARNING: OUTSIDE DATA FOLDER " << endl;
				}
				if (path.find(".frag") != std::string::npos) {
					cout << "LOAD SHADER" << endl;
					boxes.addShaderBox(path);
				}
				else if (path.find(".xml") != std::string::npos) {
					cout << "LOAD SAVEFILE" << endl;
					savedirectory = path;
					boxes.load(savedirectory);
				}
				else if (path.find(".png") != std::string::npos ||
					path.find(".jpg") != std::string::npos ||
					path.find(".JPEG") != std::string::npos
					) {
					cout << "LOAD IMAGE FILE" << endl;
					boxes.addImageBox(path);
				}
				else if (path.find(".mov") != std::string::npos ||
					path.find(".mkv") != std::string::npos ||
					path.find(".mp4") != std::string::npos ||
					path.find(".flv") != std::string::npos ||
					path.find(".vob") != std::string::npos ||
					path.find(".avi") != std::string::npos
					) {
					boxes.addVideoBox(path);
				}
			}*/
		}

		/*if (key == 'l') {
			cout << "savedirectory" << savedirectory << endl;
			if (loadAspreset) {
				boxes.load2(savedirectory);
			}else{
				boxes.load(savedirectory);
			}//boxes.update_resized(renderwidth, renderheight); //ESTA LINEA ES RE CACUIJA .
		}*/

		if (key == 's')
		{
			cout << "savedirectory" << savedirectory << endl;
			cout << "SOLO SAVE " << endl;
			boxes.save(savedirectory);
		}
		if (key == 'l')
		{
			boxes.load(savedirectory);
		}
		if (key == 'd')
		{
			isDebug = !isDebug;
			cout << "IS DEBUG " << isDebug << endl;
		}
		if (key == 'r')
		{
			boxes.reloadActiveshader();
		}
		if (key == 'w')
		{
			openRenderWindow();
		}
		//if (key == 'm') {

		//}
		if (key == 'm')
		{

			ofFbo fbo;

			fbo = *boxes.getActiverender();
			ofPixels pix;

			/*cout << "DAY " << ofGetDay() << endl;
			cout << "MONTH " << ofGetMonth() << endl;
			cout << "YEAT " << ofGetYear() << endl;
			cout << "CURRENT TIME " << ofToString(ofGetCurrentTime()) << endl;
			*/
			string txtname = "-" + ofToString(ofGetDay()) + "-" + ofToString(ofGetMonth()) + "-" + ofToString(ofGetYear()) + "-" + ofToString(ofGetHours()) + "-" + ofToString(ofGetMinutes()) + "-" + ofToString(ofGetSeconds()) + "-";
			fbo.readToPixels(pix);
			ofSaveImage(pix, "exportimgs/export" + txtname + ".png");
		}
	}
	/*if (prevKey == OF_KEY_CONTROL && key == 's') {
		cout << "lala" << endl;
	}
	if (key == OF_KEY_CONTROL) {
		cout << "PRESS CONTROL " << endl;
	}
	prevKey = key;*/
}

void ofApp::keycodePressed(ofKeyEventArgs &e)
{

	//cout << "KEY : " << e.key << endl;

	//CUANDO APRETAS CONTROL TE TOMA COMO DOS INPUTS EN EL MOMENTO.
	cout << "-------------------------------------" << endl;
	cout << "PREVKEYCODE " << prevKey << endl;
	cout << "KEYCODE : " << e.keycode << endl;
	cout << "KEY : " << e.key << endl;
	//Como que esto solo sucede cuando apretas el control y despues el save. LO SACAMOS VIENDO VALORES EN CONSOLA. NO ME PREGUNTES LA LOGICA.

	//if (prevKey == 19) {

	//Aca me lo cambio a 134 de golpe. Que onda? Hay que tener cuidado con este bug.
	if (e.key == 46 || e.key == 19)
	{

		jp_constants::set_systemDialog_open(true);
		saveas_saver.startThread();
	}

	if (prevKey == 12)
	{
		openloader.startThread();
	}

	//cout << "DELETE ALL SHADERBOXS" << endl;
	if (e.key == 'b')
	{
		boxes.clear();
		//boxes.shaderboxs.clear();
	}
	prevKey = e.keycode;
}
void ofApp::mouseDragged(int x, int y, int button)
{
	if (pantallaActiva == NODOS)
	{
		//jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
		boxes.update_mouseDragged(button);
	}
}
void ofApp::mousePressed(int x, int y, int button)
{

	if (pantallaActiva == NODOS)
	{
		jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
		boxes.update_mousePressed(button);
	}
}
void ofApp::windowResized(int w, int h)
{

	//El resize lo hace solo para mover la interfaz. Los tama�os de render se mantienen igual
	boxes.update_resized(ofGetWidth(), ofGetHeight());
	//InitGLtexture(sendertexture, ofGetWidth(), ofGetHeight()); //!?!??!!?
	//	boxes.update_resized(jp_constants::renderWidth, jp_constants::renderHeight);
}
void ofApp::keyReleased(int key) {}
void ofApp::mouseMoved(int x, int y) {}
void ofApp::mouseReleased(int x, int y, int button)
{
	if (button == 0)
	{
		//mouseButton_left = false;
	}
}
void ofApp::mouseEntered(int x, int y) {}
void ofApp::mouseExited(int x, int y) {}
void ofApp::gotMessage(ofMessage msg) {}
void ofApp::dragEvent(ofDragInfo dragInfo)
{

	cout << "WHAT " << dragInfo.position.t << endl;
	cout << "DIR : " << dragInfo.files[0] << endl;

	//ESTO TIENE QUE COINCIDIR CON LOS TAMA�OS DE LAS CAJAS QUE ACTUALMENTE ESTA EN 80x80
	float sepx = 80 * 1.4;
	float sepy = 80 * 1.6;

	float xx = dragInfo.position.x;
	float yy = dragInfo.position.y;

	int indexx = 0;
	int indexy = 0;
	for (int i = 0; i < dragInfo.files.size(); i++)
	{

		string path = dragInfo.files[i];

		if (dragInfo.files.size() > 1)
		{

			if (indexx >= ceil(sqrt(dragInfo.files.size())))
			{
				indexx = 0;
				xx = dragInfo.position.x;
				yy += sepy;
			}
			indexx++;
		}

		cout << "path " << path << endl;
		if (path.find("data") != std::string::npos)
		{
			cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
			path = path.substr(path.find("data"), path.size());
			cout << "NEW PATH CONVERSION :" << path << endl;
		}
		else
		{
			cout << "WARNING: OUTSIDE DATA FOLDER " << endl;
		}

		if (path.find(".xml") != std::string::npos &&
			!loadAspreset)
		{
			boxes.load(path);
		}
		else
		{
			boxes.addBox(path, xx, yy);
		}

		xx += sepx;
	}
}
void ofApp::openRenderWindow()
{
	if (!isRenderWindowOpen)
	{
		isRenderWindowOpen = true;
		ofGLFWWindowSettings settings;

		settings.setSize(jp_constants::window_width, jp_constants::window_height);
		settings.setPosition(ofVec2f(window_initialposx, window_initialposy));
		settings.resizable = true;
		settings.shareContextWith = mainWindow;

		windows.push_back(ofCreateWindow(settings));

		cout << "windows size" << windows.size() << endl;

		//window_width = 600;
		//window_height = 600;
		ofAddListener(windows.back()->events().draw, this, &ofApp::window_drawRender);
		ofAddListener(windows.back()->events().exit, this, &ofApp::exit);
		ofAddListener(windows.back()->events().keyPressed, this, &ofApp::window_keyPressed);
		ofAddListener(windows.back()->events().mouseMoved, this, &ofApp::window_mouseMove);
		ofAddListener(windows.back()->events().windowResized, this, &ofApp::window_resized);
	}
}
void ofApp::setInitialValues()
{

	ofXml xml;

	xml.load("settings.xml");

	auto settings = xml.getChild("settings");
	auto renderwidthaux = settings.getChild("renderwidth");
	auto renderheightaux = settings.getChild("renderheight");
	auto windowx = settings.getChild("window_x");
	auto windowy = settings.getChild("window_y");
	auto windowwidth = settings.getChild("window_width");
	auto windowheight = settings.getChild("window_height");
	auto windowopen = settings.getChild("window_open");
	auto window_fullscreenaux = settings.getChild("window_fullscreen");
#ifdef SPOUT
	auto spouton = settings.getChild("spouton");
#endif
	auto oscportin = settings.getChild("osc_port_in");
	auto oscportout = settings.getChild("osc_port_out");
	auto oscipout = settings.getChild("osc_ip_out");
	auto oscout1 = settings.getChild("oscout_mode1");
	auto oscout2 = settings.getChild("oscout_mode2");

	cout << "/****************************************************/" << endl;
	cout << "INITIAL VALUES FROM SETTINGS.XML " << endl;
	cout << "renderwidth " << renderwidthaux.getIntValue() << endl;
	cout << "renderheight " << renderheightaux.getIntValue() << endl;
	cout << "windowx " << windowx.getIntValue() << endl;
	cout << "windowy " << windowy.getIntValue() << endl;
	cout << "windowwidth " << windowwidth.getIntValue() << endl;
	cout << "windowheight " << windowheight.getIntValue() << endl;
	cout << "windowopen " << windowopen.getBoolValue() << endl;
	cout << "windowfullscreen " << window_fullscreenaux.getBoolValue() << endl;
	cout << "oscportin " << oscportin.getIntValue() << endl;
	cout << "oscportout " << oscportout.getIntValue() << endl;
	cout << "oscipout " << oscipout.getValue() << endl;
#ifdef SPOUT
	cout << "spouton " << spouton.getBoolValue() << endl;
#endif
	cout << "oscout1 " << oscout1.getBoolValue() << endl;
	cout << "oscout2 " << oscout2.getBoolValue() << endl;
	cout << "/****************************************************/" << endl;

	jp_constants::init(renderwidthaux.getIntValue(),
					   renderheightaux.getIntValue(),
					   windowwidth.getIntValue(),
					   windowheight.getIntValue());

	cout << "window_width " << jp_constants::window_width << endl;

	window_initialposx = windowx.getIntValue();
	window_initialposy = windowy.getIntValue();
	window_fullscreen = window_fullscreenaux;
#ifdef SPOUT
	spoutActive = spouton.getBoolValue();
#endif
	oscout_mode1 = oscout1.getBoolValue();
	oscout_mode2 = oscout2.getBoolValue();

	receiver.setup(oscportin.getIntValue());
	sender.setup(oscipout.getValue(), oscportout.getIntValue());
	if (windowopen.getBoolValue())
	{
		openRenderWindow();
	}
}
void ofApp::updateOSC()
{
	// hide old messages

	//RECEIVER
	// check for waiting messages
	while (receiver.hasWaitingMessages())
	{
		// get the next message
		ofxOscMessage m;
		receiver.getNextMessage(&m);
		boxes.listenToOsc(m.getAddress(), m.getArgAsFloat(0)); //Esta te levanta el OSC Digamos?
		//cout << "ADDRES:" << m.getAddress() << endl;
		//cout << "VALUE:" << m.getArgAsFloat(0) << endl;

		if (m.getAddress().find("load") != std::string::npos)
		{
			cout << "ENCONTRO LOAD " << endl;
			string dir(m.getAddress(), 6, (m.getAddress().size()));
			cout << "DIR : " << dir << endl;

			string dirfinal = "savefiles/" + dir;
			cout << "DIR FINNAL : " << dirfinal << endl;
			boxes.load(dirfinal);
			savedirectory = dirfinal;
		}
	}
	//SENDER

	//ESTO VA A HABER QUE CODEARLO MEJOR PERO VAMOS ASI POR AHORA:
	//FORMA 1 : MANDA CON NOMBRE DE CAJA TODO EL TIEMPO TODAS LAS VECESSSS
	if (oscout_mode1)
	{
		for (int i = 0; i < boxes.getBoxesSize(); i++)
		{
			for (int k = 0; k < boxes.boxes[i]->parameters.getSize(); k++)
			{
				ofxOscMessage m;
				string mensajefinal = boxes.boxes[i]->name + "/" + boxes.boxes[i]->parameters.getName(k);

				m.setAddress(mensajefinal);
				if (boxes.boxes[i]->parameters.getType(k) == boxes.boxes[i]->parameters.BOOL)
				{
					m.addBoolArg(boxes.boxes[i]->parameters.getBoolValue(k));
				}
				else
				{
					m.addFloatArg(boxes.boxes[i]->parameters.getFloatValue(k));
				}
				sender.sendMessage(m, false);
			}
		}
	}
	//FORMA 2: MANDA LOS NOMBRES COMO V1,V2,V3 Y SOLO DE LA CAJA DE LA INTERFAZ ACTIVA. :
	if (oscout_mode2)
	{
		if (boxes.openguinumber != -1)
		{
			for (int k = 0; k < boxes.boxes[boxes.openguinumber]->parameters.getSize(); k++)
			{
				ofxOscMessage m;
				string mensajefinal = "v" + ofToString(k);

				if (boxes.boxes[boxes.openguinumber]->parameters.getType(k) == boxes.boxes[boxes.openguinumber]->parameters.BOOL)
				{
					m.addBoolArg(boxes.boxes[boxes.openguinumber]->parameters.getBoolValue(k));
				}
				else
				{
					m.addFloatArg(boxes.boxes[boxes.openguinumber]->parameters.getFloatValue(k));
				}
				m.setAddress(mensajefinal);
				sender.sendMessage(m, false);
			}
		}
	}
}
//LISTENERS DE LAS VENTANAS:
void ofApp::window_drawRender(ofEventArgs &args)
{
	boxes.draw_activerender(jp_constants::window_width, jp_constants::window_height);
}
void ofApp::exit(ofEventArgs &e)
{
	cout << "EXIT WINDOW " << endl;

	//Ni idea que hacia este if ??? Pero si lo comento crashea
	if (isRenderWindowOpen)
	{
		//windows.back()->close();
	}
	windows.clear();
	window_fullscreen = false;
	isRenderWindowOpen = false;
}
void ofApp::window_mouseMove(ofMouseEventArgs &e)
{

	/*window_mousex = e.x;
	window_mousey = e.y;*/

	jp_constants::setwindow_mousex(e.x);
	jp_constants::setwindow_mousey(e.y);
}
void ofApp::window_resized(ofResizeEventArgs &args)
{
	cout << "WINDOWS RESIZED PAPA " << endl;
	cout << "WIDTH " << args.width << endl;
	cout << "HEIGHT " << args.height << endl;

	jp_constants::setwindow_width(args.width);
	jp_constants::setwindow_height(args.height);
}

void ofApp::window_keyPressed(ofKeyEventArgs &e)
{
	cout << "KEYCODE ON WINDOWS : " << e.keycode << endl;

	if (e.keycode == 70 || e.keycode == 71)
	{
		window_fullscreen = !window_fullscreen;
		windows.back()->setFullscreen(window_fullscreen);
	}
}

#ifdef SPOUT
//ESTA FUNCION CORRE EN EL SPOUT Y HACE TODO LO QUE TENGA QUE VER CON DIBUJAR EL SPOUT SENDER:
void ofApp::drawSpout()
{

	char str[256];
	ofSetColor(255);

	// ====== SPOUT =====
	// A render window must be available for Spout initialization and might not be
	// available in "update" so do it now when there is definitely a render window.
	if (!bInitialized)
	{
		// Create the sender
		bInitialized = spoutsender.CreateSender(sendername, resolution_spoutext.x, resolution_spoutext.y);
	}

	ofSetColor(255, 255, 0, 255);
	ofDrawRectangle(0, 0, resolution_spoutext.x, resolution_spoutext.y);
	ofSetColor(255, 255);
	ofDrawRectangle(0, 0, ofGetWidth() * .9, ofGetHeight() * .9);

	boxes.draw_activerender(resolution_spoutext.x, resolution_spoutext.y);

	// ====== SPOUT =====
	if (bInitialized)
	{

		if (ofGetWidth() > 0 && ofGetHeight() > 0)
		{ // protect against user minimize

			// Grab the screen into the local spout texture
			glBindTexture(GL_TEXTURE_2D, sendertexture);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, resolution_spoutext.x, resolution_spoutext.y);
			glBindTexture(GL_TEXTURE_2D, 0);

			// Send the texture out for all receivers to use
			spoutsender.SendTexture(sendertexture, GL_TEXTURE_2D, resolution_spoutext.x, resolution_spoutext.y);

			// Show what it is sending
			ofSetColor(255);
			sprintf(str, "Sending as : [%s]", sendername);
			ofDrawBitmapString(str, 20, 20);

			// Show fps
			sprintf(str, "fps: %3.3d", (int)ofGetFrameRate());
			ofDrawBitmapString(str, ofGetWidth() - 120, 20);
		}
	}
}
#endif

bool ofApp::InitGLtexture(GLuint &texID, unsigned int width, unsigned int height)
{
	if (texID != 0)
		glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}
