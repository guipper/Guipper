#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
//#include "ofxSpout2Receiver.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "../SpoutSDK/Spout.h" // Spout SDK
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
//Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_spout : public JPbox {
public:
	JPbox_spout();// constructor declared
	~JPbox_spout();

	//ofxSpout2::Receiver spoutReceiver;


	SpoutReceiver spoutreceiver; // A Spout receiver object
	ofTexture myTexture;	     // Texture used for texture share transfers

	char SenderName[256];	     // Sender name used by a receiver
	int g_Width, g_Height;       // Used for checking sender size change
	bool bInitialized;        // Initialization result


	int activesender; //ESTA ES MI VARIABLE QUE USO PARA ELEGIR EL COSO.
	//string dir;
	//JPFbohandlerGroup fbohandlergroup;

	//METODOS HEREDADOS : 
	//void reload();
	
	void setup();
	void setup(string _dir, string _name);
	void update_spout();
	void changeReciever(int);
	void update();
	void updateFBO();
	void draw();
	void clear();
	void reload();
	/*void setPos(float _x, float _y) {
	JPdragobject::setPos(_x, _y);
	//setfbohandler_nodepos();
	}*/




//	char* pointer1 = "null";
};

