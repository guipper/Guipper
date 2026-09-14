#include "ofMain.h"
#include "ofApp.h"
#include "JPutils/jp_app_paths.h"
#include <iostream>

//#include "RenderWindowApp.h"

//========================================================================
int main()
{
    // Graphics tests run in their own copied data tree, without importing a
    // real user profile. Normal launches never write into the installation.
    if (!std::getenv("GUIPPER_PERSISTENCE_TEST") && !std::getenv("GUIPPER_UISHOT")) {
        try {
            auto& paths = jp::AppPaths::current();
            paths = jp::AppPaths::discover(ofToDataPath("", true));
            paths.initialize();
            ofSetDataPathRoot(paths.data.string() + "/");
        } catch (const std::exception& error) {
            std::cerr << "Guipper could not prepare user data: " << error.what() << std::endl;
            return 1;
        }
    }


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
#ifdef TARGET_LINUX
	auto glfwWindow = dynamic_pointer_cast<ofAppGLFWWindow>(mainWindow);
	if (glfwWindow) {
		ofImage appIcon;
		if (appIcon.load("guipper.png")) {
			appIcon.setImageType(OF_IMAGE_COLOR_ALPHA);
			glfwWindow->setWindowIcon(appIcon.getPixels());
		}
	}
#endif
	shared_ptr<ofApp> mainApp(new ofApp);

	// render->main_window = mainApp;// Con esto puenteamos las apps.

	mainApp->mainWindow = mainWindow;
	// ofRunApp(RenderWindow, render);
	ofRunApp(mainWindow, mainApp);
	ofRunMainLoop();
}
