
#include "defines.h"
#ifdef SPOUT

#include "jp_box_spout.h"

JPbox_spout::JPbox_spout() {}
JPbox_spout::~JPbox_spout() {}

void JPbox_spout::setup(string _dir, string _name)
{

	JPbox::setup(_dir, _name); 
	// parameters.coutData();
	name = _name;
	dir = "spoutReceiver";
	myTexture.allocate(int(jp_constants::renderWidth), int(jp_constants::renderHeight), GL_RGBA);

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
	tipo = SPOUTBOX;

	/************************************************************************************/

	bInitialized = false; // Spout receiver initialization
	SenderName[0] = 0;	  // the name will be filled when the receiver connects to a sender
	activesender = 0;
	// Allocate a texture for shared texture transfers
	// An openFrameWorks texture is used so that it can be drawn.
}
void JPbox_spout::update_spout()
{
	char str[256];
	ofSetColor(255);
	unsigned int width, height;

	// ====== SPOUT =====
	//
	// INITIALIZE A RECEIVER
	//
	// The receiver will attempt to connect to the name it is sent.
	// Alternatively set the optional bUseActive flag to attempt to connect to the active sender.
	// If the sender name is not initialized it will attempt to find the active sender
	// If the receiver does not find any senders the initialization will fail
	// and "CreateReceiver" can be called repeatedly until a sender is found.
	// "CreateReceiver" will update the passed name, and dimensions.
	if (!bInitialized)
	{
		// spoutreceiver.
		//  Create the receiver and specify true to attempt to connect to the active sender
		if (spoutreceiver.CreateReceiver(SenderName, width, height))
		{
			// Is the size of the detected sender different ?
			if (width != g_Width || height != g_Height)
			{
				// The sender dimensions have changed so update the global width and height
				g_Width = width;
				g_Height = height;
				// Update the local texture to receive the new dimensions

				// reset render window
				// ofSetWindowShape(g_Width, g_Height);
			}
			myTexture.allocate(g_Width, g_Height, GL_RGBA);
			name = SenderName;
		//	cout << "CONECTA AL SENDER " << SenderName << endl;

			bInitialized = true;
			return; // quit for next round
		}			// receiver was initialized
		else
		{
			sprintf(str, "No sender detected");
			ofDrawBitmapString(str, 20, 20);
		}
	} // end initialization

	// The receiver has initialized so OK to draw
	if (bInitialized)
	{

		// Save current global width and height - they will be changed
		// by ReceiveTexture if the sender changes dimensions
		width = g_Width;
		height = g_Height;

		// Try to receive into the local the texture at the current size
		if (spoutreceiver.ReceiveTexture(SenderName, width, height,
										 myTexture.getTextureData().textureID, myTexture.getTextureData().textureTarget))
		{

			//	If the width and height are changed, the local texture has to be resized.
			if (width != g_Width || height != g_Height)
			{
				// Update the global width and height
				g_Width = width;
				g_Height = height;
				// Update the local texture to receive the new dimensions
				myTexture.allocate(g_Width, g_Height, GL_RGBA);
				// reset render window
				ofSetWindowShape(g_Width, g_Height);
				return; // quit for next round
			}
			// name = SenderName;
			//  Otherwise draw the texture
			myTexture.draw(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);

			// Show what it is receiving
			/*sprintf(str, "Receiving from : [%s]", SenderName);
			ofDrawBitmapString(str, 20, 20);
			sprintf(str, "RH click select sender");
			ofDrawBitmapString(str, 15, ofGetHeight() - 20);*/
		}
		else
		{
			// A texture read failure might happen if the sender
			// is closed. Release the receiver and start again.
			spoutreceiver.ReleaseReceiver();
			bInitialized = false;
			return;
		}
	}

	int activesender_prev = activesender;
	activesender = int(ofMap(parameters.getFloatValue(5), 0.0, 1.0, 0.0, spoutreceiver.GetSenderCount()));

	if (activesender_prev != activesender)
	{
		changeReciever(activesender);
	}
}

void JPbox_spout::changeReciever(int _activesender) {
	//cout << "Cantidad de spout senders " << spoutreceiver.GetSenderCount() << endl;
	for (int i = 0; i < spoutreceiver.GetSenderCount(); i++)
	{
		char* name = new char[256];
		spoutreceiver.GetSenderName(i, name);
	//	cout << "Spout sender " << i << ":" << name << endl;
		// si detecta que cambio y que el sender que elegimos es ese:
		if (i == _activesender)
		{
			// SenderName = name;
			spoutreceiver.ReleaseReceiver();
			strcpy(SenderName, name);
			bInitialized = false;
		}
	}
	//cout << "-------------------------------------" << endl;
	//cout << "activesender :" << _activesender << endl;
	//cout << "Name active sender :" << SenderName << endl;

}


void JPbox_spout::update()
{
	JPbox::update();
	update_spout();
	// The scheduler drops us to the staggered preview rate when nothing on
	// screen depends on this box. The source above is pumped either way - only
	// the render is skipped, and the FBO keeps its last frame.
	if (shouldRenderThisFrame()) updateFBO();
	// movie.update();
}
void JPbox_spout::updateFBO()
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
			/*ofSetColor(0, 255, 255, 255);
			ofDrawRectangle(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
			ofSetColor(255, 255, 0, 255);

			ofVec2f mousemap(ofMap(ofGetMouseX(),0, ofGetWidth(),0, jp_constants::renderWidth),
							 ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, jp_constants::renderHeight));
			ofDrawRectangle(mousemap,100,100);*/
			ofSetColor(255, 255);
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
void JPbox_spout::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
	/*if (mouseOverOutlet()) {
		ofSetColor(200, 200, 0, 150);
	}
	else {
		ofSetColor(0, 0, 200, 150);
	}
	ofSetColor(255, 255);*/
}
void JPbox_spout::clear()
{
	JPbox::clear();
	spoutreceiver.ReleaseReceiver();
	spoutreceiver.UnBindSharedTexture();
	//cout << "CORRE CLEAR SPOUT " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}

void JPbox_spout::reload() {
	//cout << "RELOAD DE SPOUT " << endl;
	changeReciever(activesender);
}

#endif