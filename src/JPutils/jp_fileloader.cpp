#include "jp_fileloader.h"

void DirectoryManager::loadDirectorys(){
	cout << "Carga directorios" << endl;
	cout << "-------------------------" << endl;
	loadDirectory("shaders/generative");
	loadDirectory("shaders/imageprocessing");
	loadDirectory("shaders/contrib");
	loadDirectory("shaders/blending");
	//loadDirectory("shaders/generative");

	cout << "-------------------------" << endl;
	cout << directorys[1][0] << endl;
	cout << "-------------------------" << endl;

	cout << "Termina directorios" << endl;




	

}

void DirectoryManager::loadDirectory(string _dir)
{
	string path2 = _dir;
	ofDirectory dir2(path2);
	dir2.listDir();
	vector<string> dir_shader_folder; //ARRAY DE STRINGS DE ESA CARPETA.


	for (int i = 0; i < dir2.size(); i++) {
		string compofolder_name = dir2.getName(i);
		string compofolder_path = dir2.getPath(i);
		dir_shader_folder.push_back(dir2.getPath(i));
		//cout << "Directorios generative : " << dir2.getPath(i) << endl;
	}
	//cout << "Directorios generative : " << dir_shader_folder[0] << endl;
	//cout << "Directorios SIZE : " << dir_shader_folder.size() << endl;

	directorys.push_back(dir_shader_folder);

	/*string path2 = _dir;
	//_dir
	ofDirectory dir2(path2);
	//dir2.listDir();
	vector<string> dir_shader_folder; //ARRAY DE STRINGS DE ESA CARPETA.
	
	//dir_shaders_freestyle.clear();
	for (int i = 0; i < dir2.size(); i++) {
		string compofolder_name = dir2.getName(i);
		string compofolder_path = dir2.getPath(i);
		dir_shader_folder.push_back(dir2.getPath(i));
		//cout << "Directorios Generative : " << compofolder_path << endl;
		cout << "Nombre path :  " << dir2.getPath(i) << endl;
	}
	cout << "--------------------------------------------------" << endl;

	directorys.push_back(dir_shader_folder);*/
	//cout << "DIRECTORIO SELECIONADO : " << directorys.at(0).size() << endl;
}
