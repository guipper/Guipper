#include "jp_controller.h"

JPcontroller::JPcontroller()
{
	paleta = 0;
	useTexture = false;
	//	cout << " ASLDDASD " << endl;
}
JPcontroller::~JPcontroller()
{
}
void JPcontroller::draw()
{
	// isGrabbed2;
}
void JPcontroller::update()
{
}
// ACA NO IMPORTA PORQUE TOTAL ES LO QUE USAMOS EN LOS SLIDERS
float JPcontroller::getValue()
{
	return 1;
}

/*bool JPcontroller::mouseOver() {
	if (ofGetMouseX() > x - width / 2
		&& ofGetMouseX() < x + width / 2
		&& ofGetMouseY() > y - height / 2
		&& ofGetMouseY() < y + height / 2) {
		//cout << "MOUSEOVER" << endl;
		return true;
	}
	else {
		return false;
	}
}

bool JPcontroller::mouseGrab() {
	if (mouseOver() && ofGetMousePressed()) {
		return true;
	}
	else {
		return false;
	}
}*/