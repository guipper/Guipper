

#include "defines.h"
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
	// glClearTexImage needs GL 4.4 / ARB_clear_texture, which is unavailable
	// on macOS (and not guaranteed on Linux/Mesa), where it segfaults. The
	// freshly allocated texture does not need clearing here.

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");
	parameters.addFloatValue(0.0, "reciever");
	// Appended LAST on purpose: JPboxgroup::load fills parameters positionally,
	// so inserting anywhere else would shift every parameter in every saved
	// composition. Files written before this existed simply stop short and the
	// zoom keeps its neutral 1.0.
	parameters.addFloatValue(1.0, "scale ratio");
	if (JPParameter *ratio = parameters.getJParameter(parameters.getSize()-1))
	{
		ratio->nativeMin = ratio->min = 0.1f;
		ratio->nativeMax = ratio->max = 4.0f;
		ratio->defaultFloatValue = 1.0f;
	}
	scaleRatioIndex = parameters.getSize()-1;
	tipo = NDIBOX;

	/************************************************************************************/

	bInitialized = false; // Spout receiver initialization
	SenderName[0] = 0;	  // the name will be filled when the receiver connects to a sender

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

	// The scheduler drops us to the staggered preview rate when nothing on
	// screen depends on this box. The source above is pumped either way - only
	// the render is skipped, and the FBO keeps its last frame.
	if (shouldRenderThisFrame()) updateFBO();

	// movie.update();
}
void JPbox_ndi::updateFBO()
{

	if (onoff.boolValue)
	{
		// Shared with the camera box: identical transform, one implementation.
		// At ratio 1.0 this reproduces the previous ofMap maths exactly.
		const float zoom = scaleRatioIndex >= 0 ?
			parameters.getFloatValue(scaleRatioIndex) : 1.0f;
		const jp_media::JPMediaRect r = jp_media::legacyTransformRect(
			parameters.getFloatValue(0), parameters.getFloatValue(1),
			parameters.getFloatValue(2), parameters.getFloatValue(3),
			zoom, jp_constants::renderWidth, jp_constants::renderHeight);

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();

		if (!parameters.getBoolValue(4))
		{
			ofSetColor(0, 255);
			ofDrawRectangle(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
			ofSetColor(255, 255);
			myTexture.draw(r.x, r.y, r.width, r.height);
		}
		else
		{
			// Stretch fills the canvas, but the uniform zoom still applies -
			// same as Stretch in the media boxes' transformedRect.
			const float sw = jp_constants::renderWidth * zoom;
			const float sh = jp_constants::renderHeight * zoom;
			myTexture.draw((jp_constants::renderWidth - sw) * 0.5f,
				(jp_constants::renderHeight - sh) * 0.5f, sw, sh);
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