#pragma once

#include "ofMain.h"
#include "jp_box.h"
// #include "ofxSpout2Receiver.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "ofxNDI.h" // Spout SDK
// #include "Shaderrender.h"

// #include "JPbox/JPboxgroup.h"
//  Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_ndi : public JPbox
{
public:
	JPbox_ndi(); // constructor declared
	~JPbox_ndi();

	// ofxSpout2::Receiver spoutReceiver;

	ofxNDIreceiver ndiReceiver; // NDI receiver
	ofTexture myTexture;				// Texture used for texture share transfers

	char SenderName[256];	 // Sender name used by a receiver
	int g_Width, g_Height; // Used for checking sender size change
	bool bInitialized;		 // Initialization result

	int activesender; // ESTA ES MI VARIABLE QUE USO PARA ELEGIR EL COSO.
	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	// void reload();

	void setup(string _dir, string _name);
	void update();
	void updateFBO();
	void draw();
	void clear();
	/*void setPos(float _x, float _y) {
	JPdragobject::setPos(_x, _y);
	//setfbohandler_nodepos();
	}*/

	//	char* pointer1 = "null";
};
