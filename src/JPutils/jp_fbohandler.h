#pragma once

#include "ofMain.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPutils/jp_constants.h"

// ESTA CLASE ES PARA MANEJAR LOS FBOS Y SETEAR LOS FBOS Y ESAS COSAS.
// Vamos a hacerla para que funcione con nodos.

class JPFbohandler : public JPdragobject
{
public:
	JPFbohandler();
	~JPFbohandler();

	void setup(float _x, float _y, float _width, float _height)
	{
		// fbo = nullptr;
		// fboname = nullptr;
		JPdragobject::setup(_x, _y, _width, _height);
	}
	void setName(string &_name)
	{
		name = _name;
	}
	void setFboPointer(ofFbo *_fbo, string *_fboname)
	{
		fbo = _fbo;
		isPointerSet = true;
		fboname = _fboname;
	}
	void deleteFboPointer()
	{

		// HAY QUE CORREGIR EL TEMA DE LOS PUNTEROS AMIGO. NO PUEDE SER QUE TODO ESTO CRASHEE :

		/*cout << "Borramos puntero del Fbo " << *fboname << endl;
		//Ac� hay algo sumamente extra�o.
		//fbo->clear();
		fbo->allocate(jp_constants::renderWidth, jp_constants::renderHeight);


		//if(isPointerSet){
		//fbo->destroy();
		//}

		//Esto no se si genera mas comflicto si limpio los punteros o si no los limpio. Pero vamos a dejar para
		//que por ahora los limpie.
		//fbo = nullptr;
		isPointerSet = false;
		fboname = nullptr;*/

		// fbo->destroy();
		fboname = nullptr;
		fbo = nullptr;
		isPointerSet = false;
	}

	ofFbo getFboPointer()
	{
		return *fbo;
	}
	ofFbo *getFboPointerReference()
	{
		return fbo;
	}

	string *getFboPointerNameReference()
	{
		return fboname;
	}

	string getName()
	{
		return name;
	}
	string getFboPointerName()
	{
		return *fboname;
	}

	bool isPointerSet = false;

private:
	// ofTexture * texture; //Vamos a cambiar el puntero al fbo por un puntero a una textura.
	ofFbo *fbo;
	string name;
	string *fboname;
};

class JPFbohandlerGroup
{
public:
	void addFbohandler(string _name)
	{
		// JPFbohandler * fbohandler = new JPFbohandler();
		JPFbohandler fbohandler;
		fbohandler.setName(_name);
		fbohandlers.push_back(fbohandler);
	}
	void setupdragobjects(float _x, float _y, float _width, float _height)
	{
		for (int i = 0; i < getSize(); i++)
		{
			fbohandlers[i].setup(_x, _y, _width, _height);
		}
	}
	void setPos(float _x, float _y, int _index)
	{
		fbohandlers[_index].setPos(_x, _y);
	}
	float getPosX(int _index)
	{
		return fbohandlers[_index].x;
	}
	float getPosY(int _index)
	{
		return fbohandlers[_index].y;
	}
	void setFboPointer(ofFbo *fbo, string *fboname, int _index)
	{
		fbohandlers[_index].setFboPointer(fbo, fboname);
	}
	ofFbo getFboPointer(int _index)
	{
		return fbohandlers[_index].getFboPointer();
	}
	bool mouseOver(int _index)
	{
		return fbohandlers[_index].mouseOver();
	}
	bool getisPointerSet(int _index)
	{
		return fbohandlers[_index].isPointerSet;
	}
	int getSize()
	{
		return fbohandlers.size();
	}
	void deleteFboPointer(int _index)
	{
		fbohandlers[_index].deleteFboPointer();
	}
	void clear()
	{
		for (int i = 0; i < getSize(); i++)
		{
			fbohandlers[i].deleteFboPointer();
		}
		fbohandlers.clear();
	}
	string getName(int _index)
	{
		if (getSize() >= _index)
		{
			return fbohandlers[_index].getName();
		}
		else
		{
			return "ERROR IN GETTING NAME VALUE";
		}
	}
	string getFboName(int _index)
	{
		if (fbohandlers[_index].isPointerSet)
		{
			return fbohandlers[_index].getFboPointerName();
		}
		else
		{
			return "ERROR IN GETTING NAME VALUE";
		}
	}
	ofFbo *getFboPointerReference(int _index)
	{
		return fbohandlers[_index].getFboPointerReference();
	}
	string *getFboNameReference(int _index)
	{
		if (fbohandlers[_index].isPointerSet)
		{
			return fbohandlers[_index].getFboPointerNameReference();
		}
		return nullptr;
	}
	int getPointerSetsSize()
	{
		int count = 0;
		for (int i = 0; i < getSize(); i++)
		{
			if (getisPointerSet(i))
			{
				count++;
			}
		}
		return count;
	}

private:
	vector<JPFbohandler> fbohandlers; // TODOS LOS SHADERRENDERS QUE TIENE EL OBJETO.
};