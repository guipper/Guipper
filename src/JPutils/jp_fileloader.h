#pragma once


#include "defines.h"
#include "ofMain.h"
#include "jp_constants.h"

// VAMOS A HACER 3 CLASES PARA MANEJAR ARCHIVOS.

/*OpenShaderLoader : para abrir individualmente cada archivo.
OpenSaveFileLoader : para abrir archivo de savefile
SaveAsSaver : Para guardar como.
*/

class OpenLoader : public ofThread
{
public:
	bool activeflag = false;
	string path;

	void setJPboxgroupPointer(JPboxgroup &_group)
	{
		boxes = &_group;
	}
	// SI ES UN SOLO SHADER
	//  O UN ARCHIVO DE GUARDADO DE SHADERS.
	//  O UNA IMAGEN.
	//  O UN VIDEO.
	int activeFiletype;
	enum FileType
	{
		SHADER,
		SAVEFILE,
		IMAGE,
		VIDEO
	};

private:
	JPboxgroup *boxes;

	void threadedFunction()
	{
		ofFileDialogResult result = ofSystemLoadDialog("Load file");
		if (result.bSuccess)
		{
			path = result.getPath();
			cout << "path " << path << endl;

			if (path.find(".frag") != std::string::npos)
			{
				cout << "LOAD SHADER" << endl;
				activeFiletype = SHADER;
				// boxes->addShaderBox(path);
			}
			else if (path.find(".xml") != std::string::npos)
			{
				cout << "LOAD SAVEFILE" << endl;
				activeFiletype = SAVEFILE;
				// savedirectory = openloader.path;
				// boxes->load(path);
			}
			else if (path.find(".png") != std::string::npos ||
					 path.find(".jpg") != std::string::npos ||
					 path.find(".JPEG") != std::string::npos)
			{
				cout << "LOAD IMAGE FILE" << endl;
				activeFiletype = IMAGE;
				// boxes->addImageBox(path);
			}

			else if (path.find(".mov") != std::string::npos ||
					 path.find(".mkv") != std::string::npos ||
					 path.find(".mp4") != std::string::npos ||
					 path.find(".flv") != std::string::npos ||
					 path.find(".vob") != std::string::npos ||
					 path.find(".avi") != std::string::npos)
			{
				activeFiletype = VIDEO;
			}
			activeflag = true;
		}
		jp_constants::set_systemDialog_open(false);
	}
};

/*class OpenSaveFileLoader : public ofThread {
public:
	bool activeflag = false;
	string path;
private:
	void threadedFunction() {
		cout << "LOAD FROM " << endl;
		ofFileDialogResult result = ofSystemLoadDialog("Load file");
		if (result.bSuccess) {
			path = result.getPath();
			cout << "path " << path << endl;
			activeflag = true;
		}
	}
};*/

class SaveAsSaver : public ofThread
{
public:
	bool activeflag = false;
	string path;

private:
	void threadedFunction()
	{
		ofFileDialogResult result = ofSystemSaveDialog("data.xml", "Save");
		if (result.bSuccess)
		{
			path = result.getPath();
			cout << "path " << path << endl;
			activeflag = true;
		}

		jp_constants::set_systemDialog_open(false);
	}
};
