#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_cam.h"
#include "jp_box_image.h"
#include "jp_box_shader.h"
#ifdef SPOUT
#include "jp_box_spout.h"
#endif
#include "jp_box_video.h"
#include "jp_box_framedifference.h"
//#include "ofxSpout2Receiver.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"

#ifdef SPOUT
#include "../SpoutSDK/Spout.h" // Spout SDK
#endif
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
//Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_preset : public JPbox
{
public:
	JPbox_preset(); // constructor declared
	~JPbox_preset();

	void setup(string _directory, string _name);

	//void setup(float _x, float _y, string _dirinput);
	//void setup(string _dir);

	vector<JPbox *> boxes; //ESTO SERIA UNA RELACION FRACTAL O QUE CARAJO ?
						   //string dir;
	//JPFbohandlerGroup fbohandlergroup;

	//METODOS HEREDADOS :
	//void reload();

	//void setup();
	void update();
	void updateFBO();
	void draw();
	void clear();
	void addBox(JPbox &_box);

	int activeRender;

	/*void setPos(float _x, float _y) {
	JPdragobject::setPos(_x, _y);
	//setfbohandler_nodepos();
	}*/
};
