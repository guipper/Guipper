#include "jp_box_cam.h"

JPbox_cam::JPbox_cam() {}
JPbox_cam::~JPbox_cam() {}

void JPbox_cam::setup(string _dir, string _name)
{

	name = "CAMARITA";
	dir = "cam";
	JPbox::setup(_dir, _name);

	camsize = 0;
	// COMO YA CALCULAMOS EL TIPO DE ARCHIVO QUE ES POR SU EXTENSION, ONDA LAS IMAGENES Y VIDEOS Y SHADERS.
	// PERO COMO LA CAMARA NO LO TIENE ENTONCES VAMOS A USAR LA VARIABLE DIR PARA QUE GUARDE QUE ES UNA CAMARA
	// Y AS� CUANDO LO INICIALIZA QUE CREE UNA CAM. MESPLICO. //LO MISMO EN SPOUT BOX

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addFloatValue(0.0, "camaraindex");
	parameters.addBoolValue(true, "strech");

	refreshCameraDevices();
	applyCameraIndexFromParameter(true);

	tipo = CAMBOX;
}

void JPbox_cam::refreshCameraDevices()
{
	availableDeviceIds.clear();
	vector<ofVideoDevice> devices = vidGrabber.listDevices();
	for (size_t i = 0; i < devices.size(); i++)
	{
		if (devices[i].bAvailable)
		{
			availableDeviceIds.push_back(devices[i].id);
			// log the device
			ofLogNotice() << devices[i].id << ": " << devices[i].deviceName;
		}
		else
		{
			// log the device and note it as unavailable
			ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
		}
	}
	camsize = availableDeviceIds.size();
}

void JPbox_cam::applyCameraIndexFromParameter(bool force)
{
	if (parameters.getSize() <= 4)
	{
		return;
	}
	if (availableDeviceIds.empty())
	{
		refreshCameraDevices();
	}
	if (availableDeviceIds.empty())
	{
		return;
	}

	int targetListIndex = int(std::round(ofMap(parameters.getFloatValue(4),
											 0.0f,
											 1.0f,
											 0.0f,
											 float(availableDeviceIds.size() - 1),
											 true)));
	targetListIndex = ofClamp(targetListIndex, 0, int(availableDeviceIds.size()) - 1);
	if (!force && targetListIndex == currentCameraListIndex)
	{
		return;
	}

	currentCameraListIndex = targetListIndex;
	currentDeviceId = availableDeviceIds[currentCameraListIndex];
	vidGrabber.close();
	vidGrabber.setDeviceID(currentDeviceId);
	vidGrabber.setDesiredFrameRate(60);
	vidGrabber.initGrabber(camWidth, camHeight);
}

void JPbox_cam::update()
{
	JPbox::update();

	applyCameraIndexFromParameter();
	vidGrabber.update();

	updateFBO();
}
void JPbox_cam::updateFBO()
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
		fbo.begin();
		ofClear(0, 255);
		if (!parameters.getBoolValue(5))
		{
			vidGrabber.draw(jp_constants::renderWidth / 2 - mscalex / 2 + moffsetx,
							jp_constants::renderHeight / 2 - mscaley / 2 + moffsety,
							mscalex,
							mscaley);
		}
		else
		{
			vidGrabber.draw(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
		}
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}
void JPbox_cam::draw()
{
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}

void JPbox_cam::clear()
{
	JPbox::clear();
	vidGrabber.close();
	cout << "CORRE CLEAR CAMARITA " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
