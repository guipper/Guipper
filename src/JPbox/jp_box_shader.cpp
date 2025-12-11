#include "jp_box_shader.h"

JPbox_shader::JPbox_shader() {}
JPbox_shader::~JPbox_shader() {}

void JPbox_shader::reload()
{
	// cout << "RELOD SHADER " << endl;

	JPParameterGroup auxparameters;
	auxparameters = parameters;
	// parameters.clear();

	JPFbohandlerGroup auxfbohandler;
	auxfbohandler = fbohandlergroup;

	// parameters.clear();
	// fbohandlergroup.clear();
	//	cout << "auxfbohandler ANTES" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}*/

	cout << "param size A" << parameters.getSize() << endl;

	// UF ESTO ESTA ATADO CON ALAMBRE MUY FUERTE. ACA HAY UN BUG QUE LO QUE HACE ES QUE NO RECARGUE BIEN EL SHADER.
	// BASICAMENTE LO QUE SUCEDE ES QUE CUANDO VOLVES A CARGAR Y GUARDAR A VECES NO LEVANTA LOS PARAMETROS
	// ENTONCES LE DIGO QUE LO REINICIE HASTA QUE LA CANTIDAD DE PARAMETROS SEA COMO LA CORRECTA DIGAMOS.
	// OSEA TECNICAMENTE EXISTE LA POSIBILIDAD 0.00000000000000000001% DE QUE NUNCA CARGUE BIEN Y ENTRE EN UN LOOP INFINITO DE MUERTE Y DESTRUCCION.
	// OSEA AHORA AL MENOS CARGA BIEN SIEMPRE. LO QUE NO PUEDO HACER ES QUE ME VUELVA A CARGAR LOS VALORES QUE TENIA CON LOS RENDER QUE TENIA.



	do {
		setUniforms(parameters, fbohandlergroup, dir, name);
		cout << "CANTIDAD PARAMETROS :  " << parameters.getSize() << endl;
	} while (parameters.getSize() == 0 && fbohandlergroup.getSize() == 0 && buffer.size() == 0);
	// cout << "param size D " << parameters.getSize() << endl;

	/*cout << "----------------------------------"; endl;
	cout << "Variables anteriores" << endl;
	for (int i = 0; i < auxparameters.getSize(); i++) {
		string nombre = auxparameters.getName(i);
		string valor = ofToString(auxparameters.getFloatValue(i));
		cout << "nombre : " << nombre << "valor" << valor << endl;
	}
	for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}

	cout << "Variables despues " << endl;
	for (int i = 0; i < parameters.getSize(); i++) {
		string nombre = parameters.getName(i);
		string valor = ofToString(parameters.getFloatValue(i));
		cout << "nombre : " << nombre << "valor" << valor << endl;
	}

	for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}*/
	// Esto es para que si agrego un uniform no me randomice los valores que ya estaban seteados.
	// int index = 0;
	// cout << "auxfbohandler DESPUES" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < auxfbohandler.getSize(); i++) {
		//cout << "NAME :" << auxfbohandler.getFboName(i) <<endl;
	}*/

	// cout << "fbohandlergroup" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < fbohandlergroup.getSize(); i++) {
		//cout << "NAME :" << fbohandlergroup.getFboName(i) <<endl;
	}*/
	for (int i = 0; i < auxparameters.getSize(); i++)
	{
		for (int k = 0; k < parameters.getSize(); k++)
		{
			if (parameters.getName(k) == auxparameters.getName(i))
			{
				if (parameters.getType(k) == parameters.BOOL)
				{
					parameters.setBoolValue(auxparameters.getBoolValue(i), k);
				}
				else if (parameters.getType(k) == parameters.FLOAT)
				{
					parameters.setFloatValue(auxparameters.getFloatValue(i), k);

					parameters.setFloatLerpValue(auxparameters.getLerpValue(i), k);
				}
			}
		}
	}
	/*for (int k = 0; k < parameters.getSize(); k++) {
		string nombre = parameters.getName(k);
		string valor = ofToString(parameters.getFloatValue(k));
		cout << "nombre : " << nombre << "valor" << valor << endl;
		if (parameters.getType(k) == parameters.FLOAT) {
			parameters.setFloatValue(0.0, k);
		}
	}*/
	// Esto es para que no rompa las conexiones si reinicio el shader
	// int index = 0;
	for (int i = 0; i < auxfbohandler.getSize(); i++)
	{
		for (int k = 0; k < fbohandlergroup.getSize(); k++)
		{
			if (fbohandlergroup.getName(k) == auxfbohandler.getName(i) &&
				auxfbohandler.getisPointerSet(i))
			{
				fbohandlergroup.setFboPointer(auxfbohandler.getFboPointerReference(i),
											  auxfbohandler.getFboNameReference(i), k);
			}
			else
			{
			}
		}
	}
	shader.load("shaders/default.vert", dir);

	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();
	frameNum = 0;
	
}
void JPbox_shader::reloadShaderonly()
{
	shader.load("shaders/default.vert", dir);
	frameNum = 0;
}
void JPbox_shader::setup(ofTrueTypeFont &_font,
						 string _dir,
						 string _nombre)
{
	JPbox::setup(_font);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	showCode = true;
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	/*if (!ofFile(dir).exists())
	{
		cerr << dir << " does not exist!" << endl;
		return;
	}*/

	datemodified = filesystem::last_write_time(dir);
}

void JPbox_shader::setup2(string _dir,
						  string _nombre)
{
	cout << "CORRE SETUP DE SHADER " << endl;

	JPbox::setup(_dir, _nombre);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	if (!ofFile(dir).exists())
	{
		cerr << dir << " does not exist!" << endl;
		return;
	}

	datemodified = filesystem::last_write_time(dir);
}

void JPbox_shader::setup(string _dir,
						 string _nombre)
{
	cout << "CORRE SETUP DE SHADER " << endl;
	JPbox::setup(_dir, _nombre);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	if (!ofFile(_dir).exists())
	{
		cerr << _dir << " does not exist!" << endl;
		return;
	}

	datemodified = filesystem::last_write_time(dir);
}

void JPbox_shader::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();

	// DIBUJAR NODOS:
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		ofNoFill();
		ofSetColor(0);
		ofDrawEllipse(fbohandlergroup.getPosX(i), fbohandlergroup.getPosY(i), inlet_size, inlet_size);
		ofFill();
		if (fbohandlergroup.getisPointerSet(i))
		{
			if (fbohandlergroup.mouseOver(i))
			{
				ofSetColor(100, 255, 0, 255);
			}
			else
			{
				ofSetColor(0, 120, 0, 255);
			}
		}
		else
		{
			if (fbohandlergroup.mouseOver(i))
			{
				ofSetColor(200, 0, 0, 255);
			}
			else
			{
				ofSetColor(200, 0, 0, 255);
			}
		}
		ofDrawEllipse(fbohandlergroup.getPosX(i), fbohandlergroup.getPosY(i), inlet_size, inlet_size);
	}

	ofSetColor(255, 255);
}

void JPbox_shader::clear()
{

	JPbox::clear();
	// font_p = nullptr;
	parameters.clear();

	cout << "CORRE CLEAR SHADERBOX " << endl;
	fbo.clear();
	fbo.destroy();
	// fbohandlergroup.clear();
	shader.unload();
}
void JPbox_shader::update()
{
	JPbox::update();
	updateFBO();
	frameNum++;
}
void JPbox_shader::updateFBO()
{
	// SHADER RENDER UPDATE
	if (onoff.boolValue)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();
		shader.begin();
		update_globalUniforms();
		update_NonglobalUniforms();
		ofSetColor(255, 0, 0);
		ofRect(0, 0, fbo.getWidth(), fbo.getHeight());
		
		shader.end();
		fbo.end();

		fbo.begin();
		//ofSetColor(255, 0, 0);
		//ofDrawEllipse(ofGetMouseX(), ofGetMouseY(), 50, 50);
		if (showCode) {
			// Consigue el texto del buffer
			std::string text = buffer.getText();
			float textHeight = jp_constants::h_font.stringHeight(text) ; // Asumiendo que el texto es multilinea
			//float visibleHeight = ofGetHeight() - 200; // Altura visible donde el texto se muestra

			//ofDrawBitmapString(text, 0, 0);
			jp_constants::h_font.drawString(text,
				50,
				sin(ofGetElapsedTimeMillis() * 0.00001) * textHeight/2 -textHeight/2 );
			
		}
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}
void JPbox_shader::setUniforms(JPParameterGroup &_parameters,
							   JPFbohandlerGroup &_fbohandlergroup,
							   string _dir,
							   string _name)
{
	_parameters.clear();
	_fbohandlergroup.clear();

	JPFbohandlerGroup auxfbohandler = _fbohandlergroup;
	// auxfbohandler.clear();

	vector<string> linesOfTheFile;
	 buffer = ofBufferFromFile(_dir);
	for (auto line : buffer.getLines())
	{
		linesOfTheFile.push_back(line);
	}

	_parameters.setName(_name);
	for (int l = 0; l < linesOfTheFile.size(); l++)
	{
		if (linesOfTheFile[l].rfind("uniform", 0) == 0)
		{
			if (linesOfTheFile[l].find("float") != std::string::npos)
			{
				// cout << "FLOAT NAME : " << name << endl;
				// Esta es la frase de entrada digamos.
				string fraseEntrada = linesOfTheFile[l];
				// cout << "Frase entrada " << fraseEntrada<< endl;
				// ESTO ES PARA FORMATEAR BIEN LA COSA :
				for (int i = 0; i < fraseEntrada.length(); i++)
				{
					// ESTO ES PARA PONER ESPACIOS ENTRE EL SIMBOLITO = EN CASO DE QUE NO LO TENGA.
					if (fraseEntrada[i] == '=' &&
						!(fraseEntrada[i - 1] == ' '))
					{
						fraseEntrada.insert(i++, " ");
					}
					if (fraseEntrada[i - 1] == '=' &&
						!(fraseEntrada[i] == ' '))
					{
						fraseEntrada.insert(i++, " ");
					}
					if (fraseEntrada[i] == ';' &&
						!(fraseEntrada[i - 1] == ' '))
					{
						fraseEntrada.insert(i++, " ");
					}
				}
				// cout << "Frase salida :" << fraseEntrada << endl;
				// Nos aseguramos de formatearla bien, para formatearla bien tiene que estar todo separado en palabras

				// Metemos el coso en el objeto raro ese llamado istringstream para separarlo en palabras:
				istringstream ss(fraseEntrada);
				// Traverse through all words
				// int counter = 0;
				// int total=0;
				float value = ofRandom(1);
				vector<string> frase;
				while (ss)
				{
					string word;
					ss >> word;
					// cout << "WORD " <<word << endl;
					frase.push_back(word);
				};
				// cout << "Cant contadas" << frase.size() << endl;
				if (frase.size() == 7)
				{
					// cout << "TIENE VALOR POR DEFECTO " << endl;
					value = ofToFloat(frase[4]);
					// cout << "Value : " << value << endl;
				}
				_parameters.addFloatValue(value, frase[2]);
			}
			else if (linesOfTheFile[l].find("sampler2DRect") != std::string::npos)
			{
				// cout << " SAMPLER2DRect" << endl;
				string name(linesOfTheFile[l], 22, linesOfTheFile[l].size());
				name = name.substr(0, name.find(";"));
				_fbohandlergroup.addFbohandler(name);
				auxfbohandler.addFbohandler(name);
			}
			else if (linesOfTheFile[l].find("sampler2D") != std::string::npos)
			{
				// cout << " NORMAL SAMPLER SAMPLER" << endl;
				string name(linesOfTheFile[l], 18, linesOfTheFile[l].size());
				name = name.substr(0, name.find(";"));
				// cout << "NAME " << name;
				_fbohandlergroup.addFbohandler(name);
			}
			else if (linesOfTheFile[l].find("bool") != std::string::npos)
			{
				string name(linesOfTheFile[l], 13, linesOfTheFile[l].size());
				name = name.substr(0, name.find(";"));
				// cout << "BOOL NAME : " << name << endl;
				_parameters.addBoolValue(false, name);
			}
		}
	}
}
void JPbox_shader::setfbohandler_nodepos()
{
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		float x_e = x - width / 2;
		float y_e = y;
		if (fbohandlergroup.getSize() > 1)
		{
			y_e = y + ofMap(i, 0, fbohandlergroup.getSize() - 1, -(height / 2) * 3 / 6, (height / 2) * 3 / 6);
		}
		fbohandlergroup.setPos(x_e, y_e, i);
	}
}
void JPbox_shader::update_NonglobalUniforms()
{
	for (int i = 0; i < parameters.getSize(); i++)
	{
		if (parameters.getType(i) == parameters.FLOAT)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getFloatValue(i));
		}
		else if (parameters.getType(i) == parameters.BOOL)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getBoolValue(i));
		}
	}
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		if (fbohandlergroup.getisPointerSet(i))
		{
			shader.setUniformTexture(fbohandlergroup.getName(i), fbohandlergroup.getFboPointer(i), i + 1);
		}
	}
}
void JPbox_shader::update_globalUniforms()
{
	shader.setUniformTexture("feedback", fbo.getTexture(), 0);
	/// shader.setUniformTexture("camara", videograb->getTexture(),1);
	shader.setUniform2f("resolution", fbo.getWidth(), fbo.getHeight());
	shader.setUniform4f("mouse", ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1),
						ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1),
						ofMap(jp_constants::mousePressedPos.x, 0, ofGetWidth(), 0, 1),
						ofMap(jp_constants::mousePressedPos.y, 0, ofGetHeight(), 0, 1));

	shader.setUniform1i("globalframeNum", ofGetFrameNum());
	shader.setUniform1i("boxframeNum", frameNum);
	shader.setUniform2f("window_mouse", ofMap(jp_constants::window_mousex, 0, jp_constants::window_width, 0, 1),
						ofMap(jp_constants::window_mousey, 0, jp_constants::window_height, 0, 1));
	shader.setUniform1f("time", ofGetElapsedTimef());
}

/*********************************DEPRECATED ******************************************/
/*JPParameterGroup JPbox_shader::getUniformsToJPParameterGroup(string _dir, string _name) {
	vector < string > linesOfTheFile;
	ofBuffer buffer = ofBufferFromFile(_dir);
	for (auto line : buffer.getLines()) {
		linesOfTheFile.push_back(line);
	}
	JPParameterGroup group;//TESTGROUP
	group.setName(_name);
	for (int l = 0; l < linesOfTheFile.size(); l++) {
		if (linesOfTheFile[l].rfind("uniform", 0) == 0) {
			if (linesOfTheFile[l].find("float") != std::string::npos) {
				//cout << "FLOAT NAME : " << name << endl;
				string name(linesOfTheFile[l], 14, linesOfTheFile[l].size()); // 6 letras de texto1, desde la tercera
																			  //name.substr(0, name.size() - 1);
				name.pop_back();//Le resta el ultimo caracter
				group.addFloatValue(ofRandom(1), name);
			}
			if (linesOfTheFile[l].find("sampler2D") != std::string::npos) {
				string name(linesOfTheFile[l], 18, linesOfTheFile[l].size());



			}
			if (linesOfTheFile[l].find("bool") != std::string::npos) {
				string name(linesOfTheFile[l], 13, linesOfTheFile[l].size());

				name = name.substr(0, name.find(";"));
				group.addBoolValue(false, name);
			}
		}
	}
	return group;
}*/
