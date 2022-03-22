#include "JPboxgroup.h"

JPboxgroup::JPboxgroup() {}
JPboxgroup::~JPboxgroup() {}

void JPboxgroup::setup(ofTrueTypeFont &_font, int &_activerender)
{
	font_p = &_font;
	activerender = &_activerender;

	//	cout << "WIIIIII " << jp_constants::renderWidth << endl;

	inspectorwindow_width = 450;
	inspectorwindow_x = ofGetWidth() - inspectorwindow_width / 2;
	inspectorwindow_y = inspectorwindow_height / 2;
	inspectorwindow_sepy = 30;
	inspectorwindow_height = 0; // Le tiro un valor solo para ver que onda.

	setinspectorsetactiveparams();

	// boxesdrawing.allocate(ofGetWidth(), ofGetHeight());

	offsetx = 0;
	offsety = 0;

	shaderboxagarrado = false;
	ouletagarrado = false;
	cualestaagarrado = -1;
	outlet_cualestaagarrado = -1;

	duration_mouseclick = 200;
	isDoubleClick = false;
	controllerselected = -1;
}
void JPboxgroup::draw()
{
	// boxesdrawing.draw(0, 0, ofGetWidth(), ofGetHeight());
	// boxesdrawing.draw(offsetx, offsety, ofGetWidth(), ofGetHeight());
	draw_conections();

	/*if (draw_SelectionRect) {
		ofSetColor(0,100,100);
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofNoFill();
		ofSetLineWidth(5);
		ofVec2f center = ofVec2f((ofGetMouseX()+ lastMouseClick.x)/2, (ofGetMouseY()+ lastMouseClick.y)/2);
		float w = abs(ofGetMouseX() - lastMouseClick.x) ;
		float h = abs(ofGetMouseY() - lastMouseClick.y) ;
		ofDrawRectangle(center.x, center.y,w, h);
		ofSetColor(0, 0, 255);
		ofDrawLine(ofGetMouseX(), ofGetMouseY(), lastMouseClick.x, lastMouseClick.y);
		ofSetRectMode(OF_RECTMODE_CORNER);
	}*/

	for (int i = 0; i < boxes.size(); i++)
	{
		boxes[i]->draw();
		float x = boxes[i]->x;
		float y = boxes[i]->y + boxes[i]->height / 2 - 8;
		ofDrawBitmapString(ofToString(i), x, y);
	}
	draw_paramswindow();
}
void JPboxgroup::draw_activerender()
{
	if (boxes.size() >= 1)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		boxes[*activerender]->fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
	ofImage lala;
}
void JPboxgroup::draw_activerender(float _width, float _height)
{
	if (boxes.size() >= 1)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		boxes[*activerender]->fbo.draw(0, 0, _width, _height);
	}
}
void JPboxgroup::draw_paramswindow()
{

	if (openguinumber != -1 && boxes.size() > 0)
	{
		/*//CUADRADO VERDE
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofSetColor(255, 255);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);

		//CUADRADO NEGRO ENCIMA
		float ancho2 = inspectorwindow_width * 0.98;
		float alto2 = inspectorwindow_height * 0.98;
		ofSetColor(0);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, ancho2, alto2);
		*/
		ofSetColor(120, 255);
		// constants_img::background.draw(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);

		string name = boxes[openguinumber]->name;
		ofSetColor(255);

		// DIBUJAR EL NOMBRE DEL SHADER
		// jp_constants::p2_font
		jp_constants::h_font.drawString(name,
										inspectorwindow_x - jp_constants::h_font.stringWidth(name) / 2,
										inspectorwindow_sepy); // El y de esto esta puesto medio frutanga

		int index = 0; // INDICE PARA LOS BOTONES :
		for (int i = 0; i < controllers.size(); i++)
		{
			controllers[i]->draw();
			/*if (controllers[i]->controllertype == controllers[i]->SLIDER) {
				botones_modo[index]->draw();
				//OSEA SI NO ES EL REGULAR :
				if(botones_modo[index]->
				!= 0){
					botones_speed[index]->draw();
					if (!botones_speed[index]->boolValue) {
						sliders_speed[index]->draw();
					}
				}
				index++;
				if (botones_speed[index]->boolValue) {
					sliders_speed[index]->draw()
				}
			}*/
		}
	}
}
void JPboxgroup::draw_conections()
{
	// DRAW CONECTIONS :
	ofSetLineWidth(2);
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		for (int k = boxes.size() - 1; k >= 0; k--)
		{
			for (int l = boxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
			{
				if (boxes[k]->fbohandlergroup.getFboName(l) ==
					boxes[i]->name)
				{

					boxes[i]->triangleangle = atan2(boxes[k]->fbohandlergroup.getPosY(l) - boxes[i]->outlet_y,
													boxes[k]->fbohandlergroup.getPosX(l) - boxes[i]->outlet_x);

					// boxes[i]->triangleangle+= 1;
					ofDrawLine(boxes[k]->fbohandlergroup.getPosX(l),
							   boxes[k]->fbohandlergroup.getPosY(l),
							   boxes[i]->outlet_x + boxes[i]->outlet_size / 2, boxes[i]->outlet_y);
				}
				else
				{
				}
			}
		}
	}
}
void JPboxgroup::update()
{

	// bool unoagarrado = false;
	update_paramswindow();
	float lerpAmount = 0.3;
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->update();

		// boxes[i]->parameters.update(); //La mutie y no paso nada
		for (int k = boxes[i]->parameters.getSize() - 1; k >= 0; k--)
		{
		}
		if (openguinumber == i)
		{
			for (int k = boxes[i]->parameters.getSize() - 1; k >= 0; k--)
			{
				// VAMOS A PROBAR MUTEAR ESTO A VER
				// controllers.at(k)->value = boxes[i]->parameters.getFloatValue(k);
			}
		}
		// ESTO ES PARA QUE EL SLIDER QUE REPRESENTA LA BARRA DE TIEMPO DE LOS VIDEOS
		// SE ACTUALICE
		if (boxes[i]->getTipo() == boxes[i]->VIDEOBOX && openguinumber == i &&
			!controllers.at(6)->mouseOver())
		{
			controllers.at(6)->value = boxes[i]->parameters.getFloatValue(6);
		}

		// PARA AGARRAR LAS CAJITAS :
		if (ofGetMousePressed())
		{
			if (boxes[i]->mouseOverOutlet() && !ouletagarrado && !shaderboxagarrado)
			{
				boxes[i]->activeFlag = false;
				boxes[i]->outletActiveFlag = true;
				ouletagarrado = true;
				shaderboxagarrado = false;
				outlet_cualestaagarrado = i;
				cualestaagarrado = -1;
			}
			else if (boxes[i]->mouseOver() && !ouletagarrado && !shaderboxagarrado)
			{
				cualestaagarrado = i;
				outlet_cualestaagarrado = -1;
				ouletagarrado = false;
				shaderboxagarrado = true;
				boxes[i]->activeFlag = true;
			}
		}
		// PARA QUE RECARGUE EL SHADER AUTOMATICAMENTE PAP�.
		if (!jp_constants::systemDialog_open && boxes[i]->getTipo() == boxes[i]->SHADERBOX)
		{
			time_t lasttimemodified = filesystem::last_write_time(boxes[i]->dir);
			if (lasttimemodified != boxes[i]->datemodified)
			{
				//	cout << "RELOAD SHADER " << endl;
				// cout << "-------------------------------------" << endl;
				boxes[i]->datemodified = lasttimemodified;

				// UF ESTO ESTA ATADO CON ALAMBRE MUY FUERTE. ACA HAY UN BUG QUE LO QUE HACE ES QUE NO RECARGUE BIEN EL SHADER.
				// BASICAMENTE LO QUE SUCEDE ES QUE CUANDO VOLVES A CARGAR Y GUARDAR A VECES NO LEVANTA LOS PARAMETROS
				// ENTONCES LE DIGO QUE LO REINICIE HASTA QUE LA CANTIDAD DE PARAMETROS SEA COMO LA CORRECTA DIGAMOS.
				// OSEA TECNICAMENTE EXISTE LA POSIBILIDAD 0.00000000000000000001% DE QUE NUNCA CARGUE BIEN Y ENTRE EN UN LOOP INFINITO DE MUERTE Y DESTRUCCION.
				// OSEA AHORA AL MENOS CARGA BIEN SIEMPRE. LO QUE NO PUEDO HACER ES QUE ME VUELVA A CARGAR LOS VALORES QUE TENIA CON LOS RENDER QUE TENIA.

				// cout << "Parameter a size :" << boxes[i]->parameters.getSize() << endl;
				JPbox *aux;

				boxes[i]->reload();
				cout << "RElOAD SHADER" << endl;
				// cout << "Parameter d size :" << boxes[i]->parameters.getSize() << endl;

				// boxes[i]->reloadShaderonly();
				// Este contador de uniforms hace que no crashee nada.
				int counter = 0;
				/*for (int j = 0; j < boxes[i]->parameters.getSize(); j++) {
					counter++;
				}*/
				// cout << "CONTADOR DE UNIFORMS " << counter << endl;

				if (openguinumber == i)
				{
					setControllers();
				}
				cout << "-------------------------------------" << endl;
			}
		}
	}
	// SUELTA LAS CAJITAS Y EL SELECTION RECT
	if (!ofGetMousePressed())
	{
		shaderboxagarrado = false;
		ouletagarrado = false;
		cualestaagarrado = -1;
		outlet_cualestaagarrado = -1;
		draw_SelectionRect = false;
		lastMouseClick = ofVec2f(ofGetMouseX(), ofGetMouseY());
	}
	// ESTO HABLA DE LO MAL QUE PROGRAMAS : MIRA MIRA LO QUE ES ESTO SE FUE A LA MEIRDA EL CODIGO :
	// LA VARIABLE NEEDSUPDATE DETERMINA SI EL BOTON FUE APRETADO Y SI NECEITA ACTUALIZARSE LO HACE Y LA VUELVE A SETEAR A FALSE
	// ACA LO QUE PASA ES QUE ESTO SOLUCIONA EL TEMITA DEL SYNC CUANDO SE DESPLIEGA EL BOTON PARA QUE NO QUEDE EN CUALQUIERA
	// OSEA EL NEEDSUPDATE COMUNICA QUE EL BOTON DEL COLLAPSE DE LOS SLIDERS FUE APRETADO.
	if (openguinumber != -1)
	{
		for (int k = 0; k < boxes[openguinumber]->parameters.getSize(); k++)
		{
			if (boxes[openguinumber]->parameters.parameters[k]->needsUpdate)
			{
				boxes[openguinumber]->parameters.parameters[k]->update();
				setControllers();

				boxes[openguinumber]->parameters.parameters[k]->needsUpdate = false;
			}
		}
	}
}
void JPboxgroup::update_paramswindow()
{

	int index = 0; // INDICE PARA LOS BOTONES :

	// TODO ESTO PARA QUE TIPO AGARRES UN SOLO SLIDER A LA VEZ Y NO SE VUELVA LOCO
	if (!ofGetMousePressed())
	{
		controllerselected = -1;
	}
	bool ningunaAgarrada = true;
	for (int i = 0; i < controllers.size(); i++)
	{
		controllers[i]->update();
		if (controllers[i]->activeFlag)
		{
			controllerselected = i;
			ningunaAgarrada = false;
		}
	}
	if (ningunaAgarrada)
	{
		controllerselected = -1;
	}
	for (int i = 0; i < controllers.size(); i++)
	{
		if (controllerselected == i)
		{
			controllers[i]->activable2 = true;
		}
		else
		{
			controllers[i]->activable2 = false;
		}
	}
}
void JPboxgroup::update_resized(int w, int h)
{
	cout << "RESIZE" << endl;
	cout << "w " << w << endl;
	cout << "h " << h << endl;
	// cout << "render_width" << *render_width << endl;
	// cout << "render_height" << *render_height << endl;

	/*for (int i = boxes.size()-1; i >=0 ; i--) {
		boxes[i]->fbo.clear();
		//boxes[i]->shaderrender.fbo.allocate(*render_width, *render_height);
		boxes[i]->fbo.allocate(jp_constants::renderWidth,jp_constants::renderHeight);
	}*/

	inspectorwindow_x = ofGetWidth() - inspectorwindow_width / 2;
	inspectorwindow_y = inspectorwindow_height / 2;
	inspectorwindow_sepy = 30;
	inspectorwindow_height = 0;
	setinspectorsetactiveparams();

	// int i = controllers.size()-1; i >= 0; i--
	for (int i = 0; i < controllers.size(); i++)
	{
		controllers[i]->setPos(inspectorwindow_x, inspectorwindow_height);
		inspectorwindow_height += inspectorwindow_sepy;
	}

	// setControllers();
	// boxesdrawing.allocate(ofGetWidth(), ofGetHeight());
}
void JPboxgroup::setinspectorsetactiveparams()
{

	inspectorwindow_height = 0;
	inspectorwindow_setactivesize = 25;

	inspectorwindow_height += inspectorwindow_sepy * 2.0;

	inspectorsetactive.setup(ofGetWidth() - inspectorwindow_width * 1 / 4,
							 inspectorwindow_height,
							 inspectorwindow_setactivesize * 2.,
							 inspectorwindow_setactivesize);

	inspectorwindow_height += inspectorwindow_sepy;
	inspectorreload.setup(ofGetWidth() - inspectorwindow_width * 1 / 4,
						  inspectorwindow_height,
						  inspectorwindow_setactivesize * 2.,
						  inspectorwindow_setactivesize);

	inspectorwindow_height += inspectorwindow_sepy;
}
void JPboxgroup::update_mouseDragged(int mousebutton)
{

	// Para hacer las conexiones :
	if (cualestaagarrado != -1 && boxes[cualestaagarrado]->activeFlag)
	{
		float distx = boxes[cualestaagarrado]->x - (ofGetPreviousMouseX() - ofGetMouseX());
		float disty = boxes[cualestaagarrado]->y - (ofGetPreviousMouseY() - ofGetMouseY());
		boxes[cualestaagarrado]->setPos(distx, disty);
	}
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		for (int k = boxes.size() - 1; k >= 0; k--)
		{
			for (int l = boxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
			{
				if (boxes[k]->fbohandlergroup.mouseOver(l) &&
					boxes[i]->outletActiveFlag)
				{
					boxes[k]->fbohandlergroup.setFboPointer(&boxes[i]->fbo,
															&boxes[i]->name, l);
				}
			}
		}
	}
	// Para los sliders :
	if (!shaderboxagarrado && !ouletagarrado && cualestaagarrado == -1 && outlet_cualestaagarrado == -1 && openguinumber != -1)
	{
		// Con esto detecto que no se toque ningun slider de mas: osea que no puedas estar tocando dos sliders a la vez :
		bool slideragarrado = false;
		// Si invertimos el for en este crashea. habra que cambiarlo en otro lugar tambien?
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->activeFlag)
			{
				slideragarrado = true;

				if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT && boxes[openguinumber]->parameters.getMovType(i) == 0)
				{
					// float valf = ofLerp(controllers[i]->value, boxes[openguinumber]->parameters.getFloatValue(i), 0.1);
					// boxes[openguinumber]->parameters.setFloatValue(valf, i);

					///				float slidervaluewidth = width * 3 / 4;
					/*slider_value.setup(x, y,
						slidervaluewidth,
						height * 8 / 10,
						0.0,
						1.0,
						value,
						name);
						*/
					// controllers

					// Esto es porque si esta en movtype 0 no actualiza para que no pise con el OSC , entonces hay que actualizarlo manualmente
					// Esta es la actualizaci�n manual. Acordate que si el movtype esta en 0 el valor NO SE ACTUALIZA.
					controllers[i]->value = ofMap(ofGetMouseX(), controllers[i]->x - (controllers[i]->width * 3 / 4) / 2,
												  controllers[i]->x + (controllers[i]->width * 3 / 4) / 2, 0.0, 1.0, true);

					boxes[openguinumber]->parameters.setFloatValue(controllers[i]->value, i);
					boxes[openguinumber]->parameters.setFloatLerpValue(controllers[i]->value, i);
					// boxes[openguinumber]->parameters.setSpeed(controllers[i]->speed, i);
					// boxes[openguinumber]->parameters.setMin(controllers[i]->min, i);
					// boxes[openguinumber]->parameters.setMax(controllers[i]->max, i);
				}
			}
		}
		// Si invertimos el for en este crashea. habra que cambiarlo en otro lugar tambien?
		if (!slideragarrado && openguinumber != -1)
		{
			for (int i = 0; i < controllers.size(); i++)
			{
				if ((controllers[i]->mouseOver()) && mousebutton == OF_MOUSE_BUTTON_LEFT)
				{
					if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT)
					{
						controllers[i]->activeFlag = true;
					}
				}
			}
		}
		if (!slideragarrado && !ouletagarrado)
		{
			offsetx = ofGetMouseX();
			offsety = ofGetMouseY();
		}
	}
}
void JPboxgroup::update_mousePressed(int mouseButton)
{
	////SET OPEN GUI NUMBER :

	float dif = ofGetSystemTimeMillis() - lasttime_mouseclick;
	// cout << "Diference " << dif << endl;
	isDoubleClick = (ofGetSystemTimeMillis() - lasttime_mouseclick < duration_mouseclick);
	lasttime_mouseclick = ofGetSystemTimeMillis();
	draw_SelectionRect = true;
	lastMouseClick = ofVec2f(ofGetMouseX(), ofGetMouseY());
	bool arafue = false; // POR SI NO TOCO NINGUN ELEMENTO;

	// SI EL MOUSE ESTA DENTRO DEL INSPECTOR WINDOW PAPA.
	if (mouseOverGui() && openguinumber != -1)
	{
		arafue = true;
		if (inspectorsetactive.mouseGrab())
		{
			*activerender = openguinumber;
		}
		if (inspectorreload.mouseGrab())
		{
			// reloadActiveshader();
		}
		// Checkeo todos los controles.
		bool isovercontrol = false;
		// ESTE PARECE QUE NO HACE QUE CRASHEE COMO EL RESTO DE LOS CONTROLADORES
		int index = 0; // INDEX PARA RECORRER LOS BOTONES :
		int activeone = -1;
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->mouseOver())
			{
				// cout << "MOUSE OVER " << controllers[i]->name << endl;
				isovercontrol = true;
				if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT)
				{
					controllers[i]->activeFlag = true;
					activeone = i;
				}
				else if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.BOOL)
				{
					boxes[openguinumber]->parameters.setBoolValue(controllers[i]->boolValue, i);
				}
			}
			if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT)
			{
				index++;
			}
		}
		// PONGO EN FALSE TODOS LOS QUE NO TENGO ACTIVOS:
		for (int i = 0; i < controllers.size(); i++)
		{
			if (i != activeone)
			{
				controllers[i]->activeFlag = false;
			}
			else
			{
				// Esto es porque si esta en movtype 0 no actualiza para que no pise con el OSC , entonces hay que actualizarlo manualmente
				// Esta es la actualizaci�n manual. Acordate que si el movtype esta en 0 el valor NO SE ACTUALIZA.
				controllers[i]->value = ofMap(ofGetMouseX(), controllers[i]->x - (controllers[i]->width * 3 / 4) / 2,
											  controllers[i]->x + (controllers[i]->width * 3 / 4) / 2, 0.0, 1.0, true);
				boxes[openguinumber]->parameters.setFloatValue(controllers[i]->value, i);
				boxes[openguinumber]->parameters.setFloatLerpValue(controllers[i]->value, i);
			}
		}
		if (mouseButton == 2 && isDoubleClick)
		{
			for (int i = 0; i < boxes[openguinumber]->parameters.getSize(); i++)
			{
				if (boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT)
				{
					float rdm = ofRandom(1);
					boxes[openguinumber]->parameters.setFloatLerpValue(rdm, i);
				}
			}
			setControllers();
		}
		// POR ACA VA LA COSA POR AHROA
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->overboton_collapse &&
				boxes[openguinumber]->parameters.getType(i) == boxes[openguinumber]->parameters.FLOAT)
			{
				// CAMBIA MOVTYPE
				// Bueno todo esto esta medio raro pero funciona digamos.
				cout << "Cambia movtype " << endl;
				cout << "BOTON OVER " << controllers[i]->name << endl;
			}
		}
	}
	else
	{
		randomcnt = 0;
		arafue = false;
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			// Esto esta raro:
			if (boxes[i]->mouseOverOutlet())
			{
			}
			else if (boxes[i]->mouseOver())
			{
				arafue = true;
				openguinumber = i;
				if (!mouseOverGui())
				{
					setControllers();
				}
				if (isDoubleClick)
				{
					*activerender = i;
				}
			}
		}
	}
	if (!arafue)
	{
		openguinumber = -1;
	}
	if (openguinumber != -1)
	{
		setControllers();
	}
}
void JPboxgroup::draw_cursorrect() {}
void JPboxgroup::save(string _diroutput)
{
	ofXml xml;

	auto activerender_save = xml.appendChild("activerender");
	activerender_save.set(*activerender);

	for (int i = 0; i < boxes.size(); i++)
	{

		// for (int i = boxes.size() - 1; i >= 0; i--) {
		auto data = xml.appendChild("box"); // or whatever name you want to.
		data.appendChild("nombre").set(boxes[i]->name);
		data.appendChild("x").set(boxes[i]->x);
		data.appendChild("y").set(boxes[i]->y);
		data.appendChild("directory").set(boxes[i]->dir);
		data.appendChild("onoff").set(boxes[i]->getonoff());
		// boxes[i]->parameters.coutData();
		if (boxes[i]->parameters.getSize() > 0)
		{
			auto parameters = data.appendChild("parameters");
			for (int k = 0; k < boxes[i]->parameters.getSize(); k++)
			{

				if (boxes[i]->parameters.getType(k) == boxes[i]->parameters.BOOL)
				{
					auto param = parameters.appendChild("param");
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("value").set(boxes[i]->parameters.getBoolValue(k));
				}
				else
				{
					// string name = boxes[i]->parameters.getName(k);
					auto param = parameters.appendChild("param");
					// param.set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("min").set(boxes[i]->parameters.getMin(k));
					param.appendChild("max").set(boxes[i]->parameters.getMax(k));
					param.appendChild("value").set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("movtype").set(boxes[i]->parameters.getMovType(k));
					param.appendChild("speed").set(boxes[i]->parameters.getSpeed(k));
				}
			}
		}
		if (boxes[i]->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < boxes[i]->fbohandlergroup.getSize(); k++)
			{
				if (boxes[i]->fbohandlergroup.getisPointerSet(k))
				{
					fboslinks.appendChild(boxes[i]->fbohandlergroup.getName(k))
						.set(boxes[i]->fbohandlergroup.getFboName(k));
				}
			}
		}
	}

	xml.save(_diroutput);
}
void JPboxgroup::load2(string _dirinput)
{
	JPbox_preset *presetbox = new JPbox_preset();

	// NO SE COMO HACERLO EN UNA SOLA PASADA PERO EN 2 RE FUNCA ASI QUE MIRA QUE PIOLA EH
	/*string nombre = _dirinput.substr(_dirinput.find_last_of("/\\") + 1, _dirinput.size());
	nombre = nombre.substr(0, nombre.find(".xml"));
	cout << "nombre " << nombre << endl;
	*/
	/*string name = _dirinput;
		   name = name.substr(5, name.find(".xml"));
	cout << "POSITION .XML " << name.find(".xml") << endl;
	cout << "name " << name << endl;*/
	/*presetbox->setup(ofGetMouseX(),ofGetMouseY(), _dirinput);
	presetbox->setPos(ofGetMouseX(), ofGetMouseY());
	boxes.push_back(presetbox);

	*activerender = 0;*/
}
void JPboxgroup::load(string _dirinput)
{
	clear();
	ofXml xml;

	ofDirectory dir(_dirinput);

	// if(dir.doesDirectoryExist(_dirinput)){

	xml.load(_dirinput);
	// Carga inicial de las cajitas :
	auto boxloader = xml.find("/box");

	cout << "******************************************************************" << endl;
	for (auto &box : boxloader)
	{

		auto nombre = box.getChild("nombre");
		auto x = box.getChild("x");
		auto y = box.getChild("y");
		auto directory = box.getChild("directory");
		auto onoff = box.getChild("onoff");
		// cout << "Nombre : " << nombre.getValue() << endl;
		// cout << "y : " << x.getValue() << endl;
		// cout << "x : " << y.getValue() << endl;
		// cout << "Directory : " << directory.getValue() << endl;

		JPbox *bx;
		if (directory.getValue().find(".frag") != std::string::npos)
		{
			bx = new JPbox_shader();
		}
		else if (directory.getValue().find(".jpg") != std::string::npos ||
				 directory.getValue().find(".png") != std::string::npos ||
				 directory.getValue().find(".jpeg") != std::string::npos)
		{
			bx = new JPbox_image();
		}
		else if (directory.getValue().find(".mov") != std::string::npos ||
				 directory.getValue().find(".mkv") != std::string::npos ||
				 directory.getValue().find(".mp4") != std::string::npos ||
				 directory.getValue().find(".flv") != std::string::npos ||
				 directory.getValue().find(".vob") != std::string::npos ||
				 directory.getValue().find(".avi") != std::string::npos)
		{
			bx = new JPbox_video();
		}
		else if (directory.getValue().find("cam") != std::string::npos)
		{
			bx = new JPbox_cam();
		}
#ifdef SPOUT
		else if (directory.getValue().find("spoutReceiver") != std::string::npos)
		{
			bx = new JPbox_spout();
		}
#endif
		else if (directory.getValue().find(".xml") != std::string::npos)
		{
			bx = new JPbox_preset();
		}
		else if (directory.getValue().find("framedifference") != std::string::npos)
		{
			bx = new JPbox_framedifference();
		}

		bx->setup(directory.getValue(), nombre.getValue());
		bx->setPos(x.getIntValue(), y.getIntValue());
		bx->setonoff(!box.getChild("onoff").getBoolValue());

		int index = 0;
		auto parameters = box.getChild("parameters").getChildren();
		// cout << "PARAMETER SIZE SB " << sb->parameters.getSize() << endl;
		for (auto &param : parameters)
		{
			/*cout << "............" << endl;
			cout << "nombre parametro:" << param.getChild("name").getValue() << endl;
			cout << "min parametro:" << param.getChild("min").getFloatValue() << endl;
			cout << "max parametro:" << param.getChild("max").getFloatValue() << endl;
			cout << "value parametro:" << param.getChild("value").getValue() << endl;
			cout << "movtype parametro:" << param.getChild("movtype").getIntValue() << endl;
			cout << "speed parametro:" << param.getChild("speed").getFloatValue() << endl;*/

			if (bx->parameters.getType(index) == bx->parameters.FLOAT)
			{

				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setMin(param.getChild("min").getFloatValue(), index);
				bx->parameters.setMax(param.getChild("max").getFloatValue(), index);
				bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), index);
				bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), index);
			}
			else if (bx->parameters.getType(index) == bx->parameters.BOOL)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), index);
			}
			index++;
		}
		boxes.push_back(bx);
	}
	// Una vez que cargo todas las cajitas les cargamos los links :
	// Mira lo que esta este algoritmo para levantar los links entre cajitas papa !!!
	int index1 = 0;
	int index2 = 0;
	for (auto &box : boxloader)
	{
		auto fboslinks = box.getChild("fboslinks").getChildren();
		index2 = 0;
		for (auto &fbolink : fboslinks)
		{
			for (int i = 0; i < boxes.size(); i++)
			{
				if (boxes[i]->name == fbolink.getValue() && i != index1)
				{
					ofFbo *fbopointer = &boxes[i]->fbo;
					string *fbopointername = &boxes[i]->name;
					boxes[index1]->fbohandlergroup.setFboPointer(fbopointer, fbopointername, index2);
				}
			}
			index2++;
		}
		index1++;
	}
	*activerender = xml.getChild("activerender").getIntValue();
	//}
	// activerender_loader.getIntValue();
	// activerender = activerender_loader.getIntValue();
}
void JPboxgroup::setControllers()
{

	for (int i = 0; i < controllers.size(); i++)
	{
		delete controllers[i];
		controllers[i] = nullptr;
	}
	controllers.clear();

	inspectorwindow_height = 0;
	float slider_width = inspectorwindow_width * 3 / 4;
	float slider_height = inspectorwindow_sepy * 7 / 10;

	inspectorwindow_height = font_p->stringHeight(boxes[openguinumber]->name);
	inspectorwindow_height += inspectorwindow_sepy * 1.;

	// El espacio que ponemos para dibujar el reload shader. Cosa que ya sacamos.
	/*if(boxes[openguinumber]->getTipo() == boxes[openguinumber]->SHADERBOX){
		inspectorwindow_height += inspectorwindow_setactivesize;
	}*/
	// Espacio para dibujar el set active render
	inspectorwindow_height += inspectorwindow_sepy * 0.5;
	/*inspectorwindow_height += inspectorwindow_setactivesize;
	inspectorwindow_height += inspectorwindow_sepy * 0.5;
	*/

	// FIJATE QUE ESTO SI LA PRIMERA CONDICION NO SE CUMPLE NI EVALUA LA SEGUNDA. PARA PODER EVALUAR LA SEGUNDA LA PRIMERA TIENE QUE SER TRU
	if (boxes[openguinumber]->parameters.getSize() > 0 && boxes[openguinumber]->parameters.getMovType(0) != 0)
	{
		inspectorwindow_height += inspectorwindow_sepy * 0.5;
	}

	for (int k = 0; k < boxes[openguinumber]->parameters.getSize(); k++)
	{
		if (boxes[openguinumber]->parameters.getType(k) == boxes[openguinumber]->parameters.FLOAT)
		{

			float complexsliderheight = inspectorwindow_sepy * 1.0;
			if (boxes[openguinumber]->parameters.getMovType(k) != 0)
			{
				complexsliderheight = inspectorwindow_sepy * 2.0;
			}
			if (k > 0)
			{
				if (boxes[openguinumber]->parameters.getMovType(k) != 0 &&
					boxes[openguinumber]->parameters.getMovType(k - 1) == 0)
				{
					inspectorwindow_height += inspectorwindow_sepy * 0.5;
				}
				if (boxes[openguinumber]->parameters.getMovType(k) == 0 &&
					boxes[openguinumber]->parameters.getMovType(k - 1) != 0)
				{
					inspectorwindow_height -= inspectorwindow_sepy * 0.5;
				}
			}
			// boxes[openguinumber]->parameters.setFloatValue(0.0, k);

			// boxes[openguinumber]->parameters.parameters[k]->floatValue = 0.5;

			// boxes[openguinumber]->parameters.parameters[k]->floatLerpValue = 0.5;

			// JPParameter* as = boxes[openguinumber]->parameters.parameters[k];
			JPComplexSlider *sl = new JPComplexSlider();
			sl->setup(inspectorwindow_x,
					  inspectorwindow_height, inspectorwindow_width, complexsliderheight,
					  boxes[openguinumber]->parameters.parameters[k]);

			controllers.push_back(sl);

			if (k != boxes[openguinumber]->parameters.getSize() - 1)
			{
				// inspectorwindow_height -= inspectorwindow_sepy * 0.5;
				inspectorwindow_height += complexsliderheight;
			}
			else
			{
				inspectorwindow_height += complexsliderheight * .5;
			}
		}
		else if (boxes[openguinumber]->parameters.getType(k) == boxes[openguinumber]->parameters.BOOL)
		{
			float complexsliderheight = inspectorwindow_sepy * 1.0;
			JPToogle *toogle = new JPToogle();
			toogle->setParametersPointer(boxes[openguinumber]->parameters.getJParameter(k));
			toogle->setup(inspectorwindow_x,
						  inspectorwindow_height, slider_width, slider_height, boxes[openguinumber]->parameters.getName(k), boxes[openguinumber]->parameters.getBoolValue(k));
			controllers.push_back(toogle);
			// Esto es para que no lo agregue si es el ultimo elemento :
			if (k != boxes[openguinumber]->parameters.getSize() - 1)
			{
				inspectorwindow_height += complexsliderheight;
			}
		}
	}
	inspectorwindow_y = inspectorwindow_height / 2;
}
void JPboxgroup::reloadActiveshader()
{
	if (boxes.size() > 0)
	{
		if (openguinumber != -1)
		{
			// cout << "Active Render " << *activerender <<endl;
			// cout << "Active Render " << *activerender << endl;
			boxes[openguinumber]->reload();
			setControllers();
		}
		else
		{
			// cout << "Active Render " << *activerender << endl;
			// cout << "Active Render " << *activerender << endl;
			boxes[*activerender]->reload();
		}
	}
}
void JPboxgroup::listenToOsc(string _dir, float _val)
{

	// string nombre = dir.substr(dir.find_last_of("/\\") + 1, dir.size());
	// string dir = _dir;
	string shadername = _dir.substr(_dir.find_first_of("/") + 1, _dir.find_last_of("/") - 1);
	string parametername = _dir.substr(_dir.find_last_of("/") + 1, _dir.size());

	for (int i = 0; i < boxes.size(); i++)
	{
		if (boxes[i]->name == shadername)
		{
			// cout << "COINCIDE EL NOMBRE LOCO" << endl;
			for (int k = 0; k < boxes[i]->parameters.getSize(); k++)
			{
				if (boxes[i]->parameters.getName(k) == parametername)
				{
					// cout << "COINCIDE EL PARAMETRO LOCO" << endl;
					if (boxes[i]->parameters.getType(k) == boxes[i]->parameters.FLOAT)
					{
						boxes[i]->parameters.setFloatValue(_val, k);

						// ESTO ES PARA QUE SOLO MODIFIQUE EL VALOR DEL SLIDER SOLO SI ESTA ABIERTO ESE COSO
						if (openguinumber == i)
						{
							controllers[k]->value = _val; // ACa la cantidad de controllers siempre va a ser igual a la cantidad de parameters size;
						}
					}
				}
			}
		}
	}

	if (shadername == "openguinumber")
	{

		string index = "NULL";
		// cout << "parametername.size()" << parametername.size() << endl;

		// NO TENGO NI PUTA IDEA QUE HACES ESTE CODIGO DE ACA :  ONDA . PORQUE SI ES IGUAL A 6 O A / O SEA QUE CARAJO
		if (parametername.size() == 6)
		{
			index = parametername.at(5);
		}
		if (parametername.size() == 7)
		{
			index = parametername.at(5);
			index.push_back(parametername.at(6));
		}
		int Intindex = ofToInt(index);
		// cout << "INDEX " << Intindex << endl;

		// cout << "VAL " << _val << endl;
		// float finalvalue = ofMap(_val, 0, 127, 0, 1);
		if (Intindex < controllers.size() && openguinumber != -1 && boxes[openguinumber]->parameters.getMovType(Intindex) == 0)
		{

			boxes[openguinumber]->parameters.setFloatValue(_val, Intindex);
			boxes[openguinumber]->parameters.setFloatLerpValue(_val, Intindex);
			controllers[Intindex]->value = _val;
		}
	}
}
bool JPboxgroup::mouseOverGui()
{

	if (ofGetMouseX() > inspectorwindow_x - inspectorwindow_width / 2 && ofGetMouseX() < inspectorwindow_x + inspectorwindow_width / 2 && ofGetMouseY() > inspectorwindow_y - inspectorwindow_height / 2 && ofGetMouseY() < inspectorwindow_y + inspectorwindow_height / 2)
	{

		return true;
	}
	else
	{
		return false;
	}
}
void JPboxgroup::addBox(string directory, float _x, float _y)
{
	JPbox *bx;

	// NO SE COMO HACERLO EN UNA SOLA PASADA PERO EN 2 RE FUNCA ASI QUE MIRA QUE PIOLA EH
	string nombre = directory.substr(directory.find_last_of("/\\") + 1, directory.size());
	/*Formatos de video*/
	nombre = nombre.substr(0, nombre.find(".mov"));
	nombre = nombre.substr(0, nombre.find(".mkv"));
	nombre = nombre.substr(0, nombre.find(".mp4"));
	nombre = nombre.substr(0, nombre.find(".avi"));
	nombre = nombre.substr(0, nombre.find(".vob"));
	nombre = nombre.substr(0, nombre.find(".flv"));
	/*formatos de imagen*/
	nombre = nombre.substr(0, nombre.find(".jpg"));
	nombre = nombre.substr(0, nombre.find(".png"));
	nombre = nombre.substr(0, nombre.find(".jpeg"));
	/*Formato de shader*/
	nombre = nombre.substr(0, nombre.find(".frag"));
	/*Formato de preset*/
	nombre = nombre.substr(0, nombre.find(".xml"));

	if (directory.find(".frag") != std::string::npos)
	{
		bx = new JPbox_shader();
	}
	else if (directory.find(".jpg") != std::string::npos ||
			 directory.find(".png") != std::string::npos ||
			 directory.find(".jpeg") != std::string::npos)
	{
		bx = new JPbox_image();
	}
	else if (directory.find(".mov") != std::string::npos ||
			 directory.find(".mkv") != std::string::npos ||
			 directory.find(".mp4") != std::string::npos ||
			 directory.find(".flv") != std::string::npos ||
			 directory.find(".vob") != std::string::npos ||
			 directory.find(".avi") != std::string::npos)
	{
		bx = new JPbox_video();
	}
	else if (directory.find(".xml") != std::string::npos)
	{
		bx = new JPbox_preset();
	}
	else if (directory.find("cam") != std::string::npos)
	{
		bx = new JPbox_cam();
		nombre = "CAMARITA";
	}
#ifdef SPOUT
	else if (directory.find("spoutReceiver") != std::string::npos)
	{
		bx = new JPbox_spout();
		nombre = "SPOUT";
	}
#endif SPOUT
	else if (directory.find("framedifference") != std::string::npos)
	{
		bx = new JPbox_framedifference();
		nombre = "frameDif";
	}
#ifdef NDI
	else if (directory.find("ndiReceiver") != std::string::npos)
	{
		bx = new JPbox_ndi();
		nombre = "NDI";
	}
#endif

	// ESTO ES PARA QUE NO PONGA 2 VECES EL MISMO NOMBRE:
	string nombreaux = nombre;
	bool existenombre = false;
	int counter = 2;
	do
	{
		existenombre = false;
		for (int i = 0; i < boxes.size(); i++)
		{
			// cout << "AHORA: " << nombre << endl;
			// cout << "boxes[i]->name: " << boxes[i]->name << endl;
			if (nombre.compare(boxes[i]->name) == 0)
			{
				existenombre = true;
			}
		}
		if (existenombre)
		{
			// cout << "EL NOMBRE YA EXISTE " << endl;
			nombre = nombreaux;
			nombre += ofToString(counter);
			counter++;
		}
	} while (existenombre);

	bx->setup(directory, nombre);
	bx->setPos(_x, _y);
	boxes.push_back(bx);
}
void JPboxgroup::addBox(string directory)
{
	addBox(directory, ofGetMouseX(), ofGetMouseY());
}
/*DEPRECATED : */
void JPboxgroup::setupShaderRendersFromDataFolder()
{

	string path = "shaders";
	ofDirectory dir(path);
	dir.listDir();

	if (dir.isDirectory())
	{
		for (int i = 0; i < dir.size(); i++)
		{
			string compofolder_name = dir.getName(i);
			string compofolder_path = dir.getPath(i);
			// cout << " " << compofolder_path << endl;
			ofDirectory dir2(compofolder_path);
			if (dir2.isDirectory())
			{
				dir2.listDir();
				for (int k = 0; k < dir2.size(); k++)
				{
					string compofolder_name2 = dir2.getName(k);
					string compofolder_path2 = dir2.getPath(k);
					// cout << compofolder_path2 << endl;

					JPbox_shader test;
					test.setup(*font_p,
							   compofolder_path2,
							   compofolder_name2);
					test.setPos(ofRandom(ofGetWidth() * 1 / 4, ofGetWidth() * 3 / 4),
								ofRandom(ofGetHeight() * 1 / 4, ofGetHeight() * 3 / 4));
				}
			}
		}
	}
	// cout << "--------------------------------" << endl;
}
void JPboxgroup::clear()
{

	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->clear();
		boxes[i] = nullptr;
		delete boxes.at(i);
	}

	for (int i = 0; i < controllers.size(); i++)
	{
		controllers[i] = nullptr;
		delete controllers.at(i);
	}

	openguinumber = -1;
	*activerender = 0;
	boxes.clear();
	controllers.clear();
}
void JPboxgroup::deleteSelectedShader()
{

	// YA LO ENCONTRAMOS ESE BUG :
	/*Vamos a dejar esto aca por las dudas, que me reinicie todos los dibujos cuando limpio uno.
	esto es para solucionar el tema ese de que cuando borro un fboPointer, en vez de borrarlo es como
	si me pusiera otro shader como fboPointer.  Y si por alguna raz�n le haces un clear a todos los fbos entonces
	es como si reiniciara los punteros dentro del fbohandlergroup.fbos,
	Sin embargo. Si hubiera muchisimas cajitas, asumo que hacerle un clear y un allocate a todas las cajas
	es un proceso sumamente lento. pero es lo mismo que hace en el resize as� que no s�, es posible que a futuro
	tenga que solucionarlo. Esta modificaci�n es parte del proceso por encontrar ese bug que cada tanto(todav�a
	no s� porque aparece, y hace que crashee la app, as� que medio que estamos como doblecheckeando todo e investigando
	donde mierda puede estar ese bug.
	*/

	/*for (int i = boxes.size() - 1; i >= 0; i--) {
		boxes[i]->fbo.clear();
		boxes[i]->fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
	}*/

	int index = 0;
	for (int i = 0; i < boxes.size(); i++)
	{
		if (boxes[i]->mouseOver())
		{
			int numbergrab = i;
			for (int k = boxes.size() - 1; k >= 0; k--)
			{
				// cout << "FBO HANDLER GROUP NAMES OF " << boxes[k]->name << endl;
				for (int l = 0; l < boxes[k]->fbohandlergroup.getSize(); l++)
				{
					// cout << "NUM: " << l << "  NAME : " << boxes[k]->fbohandlergroup.getFboName(l) << endl;
					if (boxes[k]->fbohandlergroup.getFboName(l) ==
						boxes[i]->name)
					{
						boxes[k]->fbohandlergroup.deleteFboPointer(l);
					}
				}
			}
			boxes[i]->clear();
			boxes[i] = nullptr;
			delete (boxes[i]);

			boxes.erase(boxes.begin() + i);
			index = i;
			if (i == openguinumber)
			{
				// openguinumber = -1;
				//*activerender = 0;
			}
		}
	}

	// ESTO ES PARA QUE SI EL QUE BORRAMOS ES EL ULTIMO AGREGADO DE LA LISTA.
	//  Y SI ESE ULTIMO AGREGADO TAMBIEN RESULTA EL RENDER ACTIVO
	//  Y QUE SI SI LA CANTIDAD DE CAJAS ES MAYOR A 0 :
	if (index == *activerender && getBoxesSize() > 0 && index == getBoxesSize())
	{
		*activerender = index - 1;
	}
	/*if(index == 1)
	else  {
		*activerender = 0;
	}*/

	// ESTO VA A ESTAR ASI HASTA QUE COMPROBEMOS QUE NO CRASHEA POR MUCHO TIEMPO.
	//*activerender = 0;
	openguinumber = -1;
}
ofTexture *JPboxgroup::getActiveTexture()
{
	if (boxes.size() >= 1)
	{
		return &boxes[*activerender]->fbo.getTexture();
		// boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
	return nullptr;
}
int JPboxgroup::getBoxesSize()
{
	return boxes.size();
}
ofFbo *JPboxgroup::getActiverender()
{
	if (boxes.size() >= 1)
	{
		return &boxes[*activerender]->fbo;
		// boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
	return nullptr;
}
/*ofFbo JPboxgroup::getActiverender() {
	if (boxes.size() >= 1) {
		return boxes[*activerender]->shaderrender.fbo;
		//boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
}*/