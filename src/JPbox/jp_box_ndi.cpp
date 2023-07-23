

#ifdef NDI
#include "jp_box_ndi.h"

JPbox_ndi::JPbox_ndi() {}
JPbox_ndi::~JPbox_ndi() {}

void JPbox_ndi::setup(string _dir, string _name)
{

	JPbox::setup(_dir, _name);
	// parameters.coutData();
	name = _name;
	dir = "ndiReceiver";
	myTexture.allocate(int(jp_constants::renderWidth), int(jp_constants::renderHeight), GL_RGBA);
	// Limpiamos el buffer de la textura ?
	glClearTexImage(myTexture.getTextureData().textureID, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");
	parameters.addFloatValue(0.0, "reciever");
	tipo = NDIBOX;

	/************************************************************************************/

	bInitialized = false; // Spout receiver initialization
	SenderName[0] = 0;		// the name will be filled when the receiver connects to a sender

	// Allocate a texture for shared texture transfers
	// An openFrameWorks texture is used so that it can be drawn.
	activesender = 0;
}
void JPbox_ndi::update()
{
	JPbox::update();
	// update_spout();
	ndiReceiver.ReceiveImage(myTexture);

	// ESTO ES PARA HACER EL CALCULO DE LOS COSITOS.
	int activesender_prev = activesender;
	activesender = int(ofMap(parameters.getFloatValue(5), 0.0, 1.0, 0.0, ndiReceiver.GetSenderCount()));
	if (activesender_prev != activesender)
	{
		cout << "Cantidad de NDI senders " << ndiReceiver.GetSenderCount() << endl;
		for (int i = 0; i < ndiReceiver.GetSenderCount(); i++)
		{
			char *name = new char[256];

			ndiReceiver.GetSenderName(name, i);
			// ndiReceiver.GetSenderName(i, name);
			cout << "NDI sender " << i << ":" << name << endl;
			// si detecta que cambio y que el sender que elegimos es ese:
			if (i == activesender)
			{
				// SenderName = name;
				ndiReceiver.ReleaseReceiver();
				strcpy(SenderName, name);
				bInitialized = false;

				if (ndiReceiver.SetSenderIndex(activesender))
				{
					cout << "Selected [" << ndiReceiver.GetSenderName(activesender) << "]" << endl;
				}
				else
				{
					cout << "Same sender" << endl;
				}
			}
		}
		cout << "-------------------------------------" << endl;
		cout << "activesender :" << activesender << endl;
		cout << "Name active sender :" << SenderName << endl;
	}

	updateFBO();

	// movie.update();
}
void JPbox_ndi::updateFBO()
{

	if (onoff.boolValue)
	{
		float mscalex = ofMap(parameters.getFloatValue(0), 0.0, 1.0, 0.0, jp_constants::renderWidth);
		float mscaley = ofMap(parameters.getFloatValue(1), 0.0, 1.0, 0.0, jp_constants::renderHeight);
		float moffsetx = ofMap(parameters.getFloatValue(2), 0.0, 1.0,
													 -jp_constants::renderWidth / 2 - mscalex / 2,
													 jp_constants::renderWidth / 2 + mscalex / 2);

		float moffsety = ofMap(parameters.getFloatValue(3), 0.0, 1.0,
													 -jp_constants::renderHeight / 2 - mscaley / 2,
													 jp_constants::renderHeight / 2 + mscaley / 2);

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();

		if (!parameters.getBoolValue(4))
		{
			ofSetColor(0, 255);
			ofDrawRectangle(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
			ofSetColor(255, 255);
			myTexture.draw(jp_constants::renderWidth / 2 - mscalex / 2 + moffsetx,
										 jp_constants::renderHeight / 2 - mscaley / 2 + moffsety,
										 mscalex,
										 mscaley);
		}
		else
		{
			myTexture.draw(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
		}
		// ofSetColor(255, 0, 0);
		// ofDrawEllipse(fbo.getWidth() / 2, fbo.getHeight() / 2, 200, 200);
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}
void JPbox_ndi::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
	if (mouseOverOutlet())
	{
		ofSetColor(200, 200, 0, 150);
	}
	else
	{
		ofSetColor(0, 0, 200, 150);
	}
	ofSetColor(255, 255);
}
void JPbox_ndi::clear()
{
	JPbox::clear();
	// spoutreceiver.ReleaseReceiver();
	// spoutreceiver.UnBindSharedTexture();
	ndiReceiver.ReleaseReceiver();
	cout << "CORRE CLEAR NDI " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}

#endif
