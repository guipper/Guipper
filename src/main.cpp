#include "ofMain.h"
#include "ofApp.h"
//#include "RenderWindowApp.h"

//========================================================================
int main()
{

	/*ofSetupOpenGL(1024, 768, OF_WINDOW);			// <-------- setup the GL context
	ofRunApp(new ofApp());
	*/

	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 2);

	// Render Window
	settings.setSize(1200, 720); // Importa el tama�o si despues lo pongo en fullscreen? No, no?
	settings.setPosition(ofVec2f(400, 300));

	settings.resizable = true;
	// shared_ptr<ofAppBaseWindow> RenderWindow = ofCreateWindow(settings);
	// shared_ptr<RenderWindowApp> render(new RenderWindowApp);

	// Gui window
	settings.setSize(100, 100);
	settings.setPosition(ofVec2f(300, 300));
	settings.resizable = true;
	// settings.shareContextWith = RenderWindow;
	shared_ptr<ofAppBaseWindow> mainWindow = ofCreateWindow(settings);
	shared_ptr<ofApp> mainApp(new ofApp);

	// render->main_window = mainApp;// Con esto puenteamos las apps.

	mainApp->mainWindow = mainWindow;
	// ofRunApp(RenderWindow, render);
	ofRunApp(mainWindow, mainApp);
	ofRunMainLoop();
}
