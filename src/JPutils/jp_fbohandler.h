#pragma once

#include "defines.h"
#include "ofMain.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPutils/jp_constants.h"
#include <utility>

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

	void swapConnection(JPFbohandler &other)
	{
		std::swap(fbo, other.fbo);
		std::swap(fboname, other.fboname);
		std::swap(isPointerSet, other.isPointerSet);
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

	string getName() const
	{
		return name;
	}
	string getFboPointerName() const
	{
		return *fboname;
	}

	bool isPointerSet = false;

private:
	// ofTexture * texture; //Vamos a cambiar el puntero al fbo por un puntero a una textura.
	ofFbo *fbo = nullptr;
	string name;
	string *fboname = nullptr;
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
	bool swapConnections(int firstIndex, int secondIndex)
	{
		if (firstIndex < 0 || secondIndex < 0 ||
			firstIndex >= getSize() || secondIndex >= getSize() ||
			firstIndex == secondIndex)
		{
			return false;
		}
		fbohandlers[firstIndex].swapConnection(fbohandlers[secondIndex]);
		return true;
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
	string getName(int _index) const
	{
		if (_index >= 0 && _index < (int)fbohandlers.size())
		{
			return fbohandlers[_index].getName();
		}
		else
		{
			return "ERROR IN GETTING NAME VALUE";
		}
	}
	int findIndexByName(const string &name) const
	{
		for (int i = 0; i < (int)fbohandlers.size(); i++)
		{
			if (fbohandlers[i].getName() == name)
			{
				return i;
			}
		}
		return -1;
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
