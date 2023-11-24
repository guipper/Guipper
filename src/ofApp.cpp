#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

	font_p.loadFont("font/Montserrat-Regular.ttf", 11); // Inicio fuente.

	ofSetVerticalSync(false);
	// FreeConsole();

	ofDisableArbTex();
	// ESTAS COSAS LAS SETEA DESDE EL SETTINGS XML DESPUES. PERO ACA LES DA UN VALOR INICIAL  POR LAS DUDAS :
	float altoventana = ofGetScreenHeight() * 3 / 4;
	float anchoventana = ofGetScreenWidth() * 3 / 4;
	ofSetWindowShape(anchoventana, altoventana);
	ofSetWindowPosition(ofGetScreenWidth() / 2 - anchoventana / 2,
						ofGetScreenHeight() / 2 - altoventana / 2);

	/*ofSetWindowPosition(ofGetScreenWidth() / 2 - anchoventana / 2 - ofGetScreenWidth(),
		ofGetScreenHeight() / 2 - altoventana / 2);
	*/

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


	dirmanager.loadDirectorys();


	receiver.setup(PORT);
	oscout_mode1 = true;
	oscout_mode2 = true;

	loadSettings();

	ofSetWindowTitle("GUIPPER");

	loadAspreset = false;
	//.----------------------------------------------------------------/

#ifdef NDI
	// INIT NDI :
	//  Optionally set fbo readback using OpenGL pixel buffers
	ndiSender.SetReadback(); // Change to false to compare
	// Optionally set NDI asynchronous sending
	// instead of clocked at the specified frame rate (60fps default)
	ndiSender.SetAsync();
	// ndiSender.
	ndiSender.CreateSender(sendername, jp_constants::renderWidth, jp_constants::renderHeight);
//---------------------------------------------------/
#endif

	pantallaActiva = NODOS;

	// INIT SPOUT SENDER :
	ofBackground(10, 100, 140);
	ofEnableNormalizedTexCoords(); // explicitly normalize tex coords for ofBox

	strcpy(sendername, "Guipper"); // Set the sender name
	ofSetWindowTitle(sendername);  // show it on the title bar

// ====== SPOUT =====
#ifdef SPOUT
	bInitialized = false; // Spout sender initialization

	// Create an OpenGL texture for data transfers
	sendertexture = 0; // make sure the ID is zero for the first time
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	// OK . Esto tira error si no pasas otros valores que no sean : ofGetWidth(), ofGetHeight().
	// HAbria que ver por que . . . . . Y si le tiras un resize la mata.
	// cout << "ANCHO VENTANA " << ofGetWidth() << endl;//1440
	// cout << "ALTO VENTANA " << ofGetHeight() << endl;//810
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?

	resolution_spoutext = ofVec2f(jp_constants::renderWidth, jp_constants::renderHeight);
	// resolution_spoutext = ofVec2f(ofGetWidth(),ofGetHeight());

	// LA RECONCHA DEL PATO, NO HAY MANERA POSIBLE PARA QUE PASE EL SPOUT EN UNA RESOLUCION
	// MAS GRANDE QUE EN LA VENTANA DEL ORTO!??!?!?!?!?!
	//InitGLtexture(sendertexture,1920, 1080); //!?!??!!?
#endif

	// 3D drawing setup for a sender
	// glEnable(GL_DEPTH_TEST);							// enable depth comparisons and update the depth buffer
	// glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
	//ofDisableArbTex(); // needed for textures to work
	// myTextureImage.loadImage("SpoutBox1.png");			// Load a texture image for the demo
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
		cout << "Save session to " << savedirectory << endl;
		boxes.save(savedirectory);
		saveas_saver.activeflag = false;
	}

	// PARA MANEJAR UN PROCESO PARALELO QUE CARGA LOS ARCHIVOS.
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
		// outletimg.draw(ofGetWidth() / 2, ofGetHeight() / 2, 200, 200);
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
	font_p.drawString("Active Render :" + ofToString(activerender), 30, posy -= sepy);
	font_p.drawString("FPS :" + ofToString(ofGetFrameRate()), 30, posy -= sepy);
	font_p.drawString("Boxes size : " + ofToString(boxes.getBoxesSize()), 30, posy -= sepy);
	font_p.drawString("Active Sequence : " + ofToString(boxes.activeSequence), 30, posy -= sepy);
	//font_p.drawString("DIALOG BOX : " + ofToString(jp_constants::systemDialog_open), 30, posy -= sepy);
}
void ofApp::draw_instrucciones()
{
	float x = 30;
	float y = 30;
	float sepy = 30;

	float x2 = 30;
	float y2 = 30;

	ofSetColor(0);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
	ofSetColor(255);
	
	//AGREGAR BOTON EN ALGUN MOMENTO
	if (language == 0){
		jp_constants::p_font.drawString("INSTRUCCIONES : ", x, y += sepy);
		jp_constants::p_font.drawString("Para cargar archivo (todos) arrastrarlo hasta la ventana", x, y += sepy);
		jp_constants::p_font.drawString("TECLAS : ", x, y += sepy);
		jp_constants::p_font.drawString("t : Cambia ente cargar un xml como preset o como compo", x, y += sepy);
		jp_constants::p_font.drawString("w : Abre ventana aparte", x, y += sepy);
		jp_constants::p_font.drawString("f : Sobre la ventana aparte abierta : FullScreen", x, y += sepy);
		jp_constants::p_font.drawString("s : Guardar sobre el xml actual : " + savedirectory, x, y += sepy);
		jp_constants::p_font.drawString("ctrl+s : Genera un nuevo archivo XML(guardarlo con la extension correcta)", x, y += sepy);
		jp_constants::p_font.drawString("d : Muestra los datos de debug", x, y += sepy);
		jp_constants::p_font.drawString("h : Agrega caja SPOUT INPUT", x, y += sepy);
		jp_constants::p_font.drawString("c : Agrega caja camara", x, y += sepy);
		jp_constants::p_font.drawString("m : Exportar imagen", x, y += sepy);
		jp_constants::p_font.drawString("e : Activar modo secuencia", x, y += sepy);

	
	
	}
	if (language == 1) {
		jp_constants::p_font.drawString("INSTRUCTIONS : ", x, y += sepy);
		jp_constants::p_font.drawString("To load any file drag it to this window(any kind)", x, y += sepy);
		jp_constants::p_font.drawString("KEYS : ", x, y += sepy);
		jp_constants::p_font.drawString("t : Change between loading a box as a preset or as a full compo", x, y += sepy);
		jp_constants::p_font.drawString("w : Opens another window", x, y += sepy);
		jp_constants::p_font.drawString("f : Click over the another window to make it fullscreen", x, y += sepy);
		jp_constants::p_font.drawString("s : Save on the actual XML : " + savedirectory, x, y += sepy);
		jp_constants::p_font.drawString("ctrl+s : Generates a new XML (save it with the correct extension please)", x, y += sepy);
		jp_constants::p_font.drawString("d : Show debug data", x, y += sepy);
		jp_constants::p_font.drawString("h : Add spout input box", x, y += sepy);
		jp_constants::p_font.drawString("c : Add Camera box", x, y += sepy);
		jp_constants::p_font.drawString("m : Export image to exportimgs folder", x, y += sepy);
		jp_constants::p_font.drawString("e : Activate sequence mode", x, y += sepy);
		jp_constants::p_font.drawString("OSC COMMANDS ", x, y += sepy);
		jp_constants::p_font.drawString("/load/(dir) : load directory of a savefile", x, y += sepy);
		jp_constants::p_font.drawString("/setactiverender/(num) : set active render", x, y += sepy);
		jp_constants::p_font.drawString("/openguinumber/(value) : control de active", x, y += sepy);
		jp_constants::p_font.drawString("/(name of shader)/(name of param) : to control using by name", x, y += sepy);
	}

	
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
// Esta es la que se dibuja en la otra ventana
void ofApp::drawRender()
{
	boxes.draw_activerender();
}
void ofApp::keyPressed(int key)
{

	// keyIsDown[key] = true;

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
			// ofToggleFullscreen();
		}
		if (key == 'h')
		{
#ifdef SPOUT
			boxes.addBox("spoutReceiver");
#else
			cerr << "Spout not supported" << endl;
#endif
		}
		if (key == 'i')
		{
			boxes.addBox("framedifference");
		}
		if (key == 'c')
		{
			// boxes.addCamBox();
			boxes.addBox("cam");
		}
		if (key == 'n')
		{
#ifdef NDI
			boxes.addBox("ndiReceiver");
#else
			cerr << "NDI not supported" << endl;
#endif
		}

		if (key == OF_KEY_DEL)
		{
			cout << "DEL " << endl;
			boxes.deleteSelectedShader();
		}

		if (key == 'o')
		{
			// openloader.startThread();
			// ESTO ES LO QUE HABRIA QUE PROBAR EN MAC PARA VER SI FUNCA O NO . CUANDO ESTEMOS AH�
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
			cout << "Save session to " << savedirectory << endl;
			boxes.save(savedirectory);
		}
		if (key == 'l')
		{
			cout << "Load session from " << savedirectory << endl;
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
		// if (key == 'm') {

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
		if (key == 'e'){
			boxes.activeSequence = !boxes.activeSequence;
		}

		if (key == 'z') {

			//boxes.load(dirmanager.directorys[0][0]);
			//boxes.addBox("data/shaders/generative/nubes.frag",300,400);
			
			cout << "DIRECTORIO LOCO " << dirmanager.directorys[0][0] << endl;
			string rutita = "D:/of_v0.11.2_vs2017_release/apps/myApps/guipper3/bin/data/shaders/generative/nubes.frag";
			if (rutita.find("data") != std::string::npos) {
				cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
				rutita = rutita.substr(rutita.find("data"), rutita.size());
				cout << "NEW PATH CONVERSION :" << rutita << endl;
			}

			boxes.addBox(rutita, ofGetWidth()/2, ofGetHeight()/2); //BUENO ESTO FUNCIONA.





		}
		if (key == 'x') {

		
		}
		if(key == 'c'){
		
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

	// cout << "KEY : " << e.key << endl;

	// CUANDO APRETAS CONTROL TE TOMA COMO DOS INPUTS EN EL MOMENTO.
	cout << "-------------------------------------" << endl;
	cout << "PREVKEYCODE " << prevKey << endl;
	cout << "KEYCODE : " << e.keycode << endl;
	cout << "KEY : " << e.key << endl;
	// Como que esto solo sucede cuando apretas el control y despues el save. LO SACAMOS VIENDO VALORES EN CONSOLA. NO ME PREGUNTES LA LOGICA.

	// if (prevKey == 19) {

	// Aca me lo cambio a 134 de golpe. Que onda? Hay que tener cuidado con este bug.
	if (e.key == 46 || e.key == 19)
	{

		jp_constants::set_systemDialog_open(true);
		saveas_saver.startThread();
	}

	if (prevKey == 12)
	{
		openloader.startThread();
	}

	// cout << "DELETE ALL SHADERBOXS" << endl;
	if (e.key == 'b')
	{
		boxes.clear();
		// boxes.shaderboxs.clear();
	}
	prevKey = e.keycode;
}
void ofApp::mouseDragged(int x, int y, int button)
{
	if (pantallaActiva == NODOS)
	{
		// jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
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
	if (pantallaActiva == TUTORIAL) {
		if (language == 0) {
			language = 1;
		}
		else if (language == 1) {
			language = 0;
		}
	}
}
void ofApp::windowResized(int w, int h)
{

	// El resize lo hace solo para mover la interfaz. Los tama�os de render se mantienen igual
	boxes.update_resized(ofGetWidth(), ofGetHeight());
	// InitGLtexture(sendertexture, ofGetWidth(), ofGetHeight()); //!?!??!!?
	//	boxes.update_resized(jp_constants::renderWidth, jp_constants::renderHeight);
}
void ofApp::keyReleased(int key) {}
void ofApp::mouseMoved(int x, int y) {}
void ofApp::mouseReleased(int x, int y, int button)
{
	if (button == 0)
	{
		// mouseButton_left = false;
	}
}
void ofApp::mouseEntered(int x, int y) {}
void ofApp::mouseExited(int x, int y) {}
void ofApp::gotMessage(ofMessage msg) {}
void ofApp::dragEvent(ofDragInfo dragInfo)
{

	cout << "WHAT " << dragInfo.position.t << endl;
	cout << "DIR : " << dragInfo.files[0] << endl;

	// ESTO TIENE QUE COINCIDIR CON LOS TAMA�OS DE LAS CAJAS QUE ACTUALMENTE ESTA EN 80x80
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
#ifdef RELATIVEDIRS
		cout << "path " << path << endl;
		if (path.find("data") != std::string::npos) {
			cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
			path = path.substr(path.find("data"), path.size());
			cout << "NEW PATH CONVERSION :" << path << endl;
		}
		else {
			cout << "WARNING: OUTSIDE DATA FOLDER " << endl;
		}
#endif
		cout << "path " << path << endl;
		if (path.find(".xml") != std::string::npos &&
			!loadAspreset){
			savedirectory = path;
			boxes.load(path);
		}
		else{
			
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

		// window_width = 600;
		// window_height = 600;
		ofAddListener(windows.back()->events().draw, this, &ofApp::window_drawRender);
		ofAddListener(windows.back()->events().exit, this, &ofApp::exit);
		ofAddListener(windows.back()->events().keyPressed, this, &ofApp::window_keyPressed);
		ofAddListener(windows.back()->events().mouseMoved, this, &ofApp::window_mouseMove);
		ofAddListener(windows.back()->events().windowResized, this, &ofApp::window_resized);
	}
}
void ofApp::loadSettings()
{

	const auto settingsPath = ofToDataPath("settings.xml");

	// If file does not exist, do not change anything (use default values)
	if (!ofFile(settingsPath).exists())
		return;

	ofXml xml;
	xml.load(settingsPath);

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
	auto durationgallery = settings.getChild("durationgallery");

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
	cout << "durationgallery " << durationgallery.getFloatValue() << endl;
	cout << "/****************************************************/" << endl;

	jp_constants::init(renderwidthaux.getIntValue(),
					   renderheightaux.getIntValue(),
					   windowwidth.getIntValue(),
					   windowheight.getIntValue());
	jp_constants::setdurationgallery(durationgallery.getFloatValue());

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
std::string toXmlString(const bool value)
{
	return value ? "true" : "false";
}
void ofApp::saveSettings()
{
	const auto settingsPath = ofToDataPath("settings.xml");

	ofXml xml;

	auto settings = xml.appendChild("settings");
	settings.appendChild("renderwidth").set(jp_constants::renderWidth);
	settings.appendChild("renderheight").set(jp_constants::renderHeight);
	settings.appendChild("durationgallery").set(jp_constants::durationgallery);
	settings.appendChild("window_x").set(ceil(window_initialposx));
	settings.appendChild("window_y").set(ceil(window_initialposy));
	//settings.appendChild("window_width").set(jp_constants::window_width);
	//settings.appendChild("window_height").set(jp_constants::window_height)s;
	//settings.appendChild("window_fullscreen").set(toXmlString(window_fullscreen));

	settings.appendChild("window_width").set(600);
	settings.appendChild("window_height").set(600);
	settings.appendChild("window_fullscreen").set(false);
	settings.appendChild("window_open").set(toXmlString(isRenderWindowOpen));
#ifdef SPOUT
	settings.appendChild("spouton").set(toXmlString(spoutActive));
#endif
	settings.appendChild("osc_port_in").set(receiver.getPort());
	settings.appendChild("osc_port_out").set(sender.getPort());
	settings.appendChild("osc_ip_out").set(sender.getHost());
	settings.appendChild("oscout_mode1").set(toXmlString(oscout_mode1));
	settings.appendChild("oscout_mode2").set(toXmlString(oscout_mode2));

	xml.save(settingsPath);
}
void ofApp::updateOSC(){
	// hide old messages

	// RECEIVER
	//  check for waiting messages
	while (receiver.hasWaitingMessages())
	{
		// get the next message
		ofxOscMessage m;
		receiver.getNextMessage(&m);
		boxes.listenToOsc(m.getAddress(), m.getArgAsFloat(0)); // ACA PROCESA TODO EL OSC DE LAS CAJAS BASICAMENTE TODO
		// cout << "ADDRES:" << m.getAddress() << endl;
		// cout << "VALUE:" << m.getArgAsFloat(0) << endl;
		cout << "LLEGA OSC  " << m.getAddress() << endl;
		if (m.getAddress().find("load") != std::string::npos)
		{
			cout << "ENCONTRO LOAD " << endl;
			string dir(m.getAddress(), 5, (m.getAddress().size()));
			cout << "DIR : " << dir << endl;

			string dirfinal = "savefiles/" + dir;
			cout << "DIR FINNAL : " << dirfinal << endl;
			boxes.load(dirfinal);
			savedirectory = dirfinal;
		}
	}



	// SENDER
	// ESTO VA A HABER QUE CODEARLO MEJOR PERO VAMOS ASI POR AHORA:
	// FORMA 1 : MANDA CON NOMBRE DE CAJA TODO EL TIEMPO TODAS LAS VECESSSS
	if (oscout_mode1){
		for (int i = 0; i < boxes.getBoxesSize(); i++){
			for (int k = 0; k < boxes.boxes[i]->parameters.getSize(); k++){
				ofxOscMessage m;
				string mensajefinal = boxes.boxes[i]->name + "/" + boxes.boxes[i]->parameters.getName(k);
				m.setAddress(mensajefinal);
				if (boxes.boxes[i]->parameters.getType(k) == boxes.boxes[i]->parameters.BOOL){
					m.addBoolArg(boxes.boxes[i]->parameters.getBoolValue(k));
				}
				else{
					m.addFloatArg(boxes.boxes[i]->parameters.getFloatValue(k));
				}
				sender.sendMessage(m, false);
			}
		}
	}

	//FORMA 2: MANDA LOS NOMBRES COMO V1,V2,V3 Y SOLO DE LA CAJA DE LA INTERFAZ ACTIVA. :
	if(oscout_mode2){
		if (boxes.openguinumber != -1){
			for (int k = 0; k < boxes.boxes[boxes.openguinumber]->parameters.getSize(); k++){
				ofxOscMessage m;
				string mensajefinal = "v" + ofToString(k);
				if (boxes.boxes[boxes.openguinumber]->parameters.getType(k) == boxes.boxes[boxes.openguinumber]->parameters.BOOL){
					m.addBoolArg(boxes.boxes[boxes.openguinumber]->parameters.getBoolValue(k));
				}
				else{
					m.addFloatArg(boxes.boxes[boxes.openguinumber]->parameters.getFloatValue(k));
				}
				m.setAddress(mensajefinal);
				sender.sendMessage(m, false);
			}
		}
	}

}
void ofApp::exit() {
	//cout << "LALA" << endl;
	//windows.back()->close();
}

// LISTENERS DE LAS VENTANAS:
void ofApp::window_drawRender(ofEventArgs &args)
{
	boxes.draw_activerender(jp_constants::window_width, jp_constants::window_height);
}
void ofApp::exit(ofEventArgs &e)
{
	cout << "EXIT WINDOW " << endl;
	// Save settings before exiting
	saveSettings();

	// Ni idea que hacia este if ??? Pero si lo comento crashea
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
// ESTA FUNCION CORRE EN EL SPOUT Y HACE TODO LO QUE TENGA QUE VER CON DIBUJAR EL SPOUT SENDER:
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

	//ofSetColor(255, 255, 0, 255);
	//ofDrawRectangle(0, 0, resolution_spoutext.x, resolution_spoutext.y);
	//ofSetColor(255, 255);
	//ofDrawRectangle(0, 0, ofGetWidth() * .9, ofGetHeight() * .9);

	//boxes.draw_activerender(resolution_spoutext.x, resolution_spoutext.y);

	// ====== SPOUT =====
	if (bInitialized)
	{

		if (ofGetWidth() > 0 && ofGetHeight() > 0)
		{ // protect against user minimize

			ofFbo& fbo = *boxes.getActiverender();
			
			fbo.begin();
			ofPushMatrix();
			ofScale(1, -1, 1); // Invierte el eje Y
			ofTranslate(0, -fbo.getHeight()); // Desplaza para compensar la inversión
			// ... dibuja tu contenido aquí ...
			ofPopMatrix();
			fbo.end();

			GLuint texID = fbo.getTexture().getTextureData().textureID;
			spoutsender.SendTexture(texID, GL_TEXTURE_2D, resolution_spoutext.x, resolution_spoutext.y);

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
