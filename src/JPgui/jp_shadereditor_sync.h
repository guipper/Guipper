#pragma once

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ofMain.h"

// Puente entre la pantalla IMPORT de Guipper y la biblioteca online de
// jpshadereditor.
//
// El endpoint público NO entrega el código en el índice (/api/shaders devuelve
// ~685 entradas livianas, sin `contenido`), así que el sync hace dos pasadas:
// el índice completo en un hilo de red y, por cada shader, el detalle. El
// trabajo con OpenGL (compilar y dibujar de prueba) queda SIEMPRE en el hilo
// principal, en update(), porque un ofShader necesita el contexto vivo.
namespace jp_shadereditor
{
	// Host y raíz del servicio. Se dejan acá y no en el llamador para que la app
	// y las pruebas hablen con el mismo backend.
	constexpr const char *kApiRoot = "https://vps-4455523-x.dattaweb.com/jpshadereditor/api";
	constexpr const char *kPublicUrl = "https://fullscreencode.com/jpshadereditor/shader.html?shader=";

	struct RemoteShader
	{
		std::string name;
		std::string author;
		std::string updated;
		std::string thumbnail;
		int passes = 0;
		bool compiles = false;
	};

	struct ImportResult
	{
		std::string name;
		std::string file;
		std::string error;
		int parameters = 0;
		bool updated = false;
		bool multipass = false;
	};

	// Traducción pura de texto: documento de jpshadereditor -> fragment GLSL 330
	// autocontenido para Guipper. Devuelve vacío y llena `error` cuando el
	// documento no se puede representar como un solo pase.
	std::string translate(const std::string &name, const ofJson &detail, std::string &error);

	// Nombre de archivo seguro y estable para un shader remoto.
	std::string fileNameFor(const std::string &name);

	class Sync
	{
	public:
		enum class Status
		{
			Idle,
			Catalog,
			Importing,
			Finished,
			Failed
		};

		Sync() = default;
		~Sync();
		Sync(const Sync &) = delete;
		Sync &operator=(const Sync &) = delete;

		// Arranca (o reinicia) el sync. `directory` es absoluto. Con `force`
		// vuelve a bajar lo que el manifiesto ya daba por al día.
		void start(const std::string &apiRoot, const std::string &directory, bool force);
		void cancel();

		// Hilo principal, con contexto GL: traduce, escribe, compila y hace el
		// dibujo de prueba de lo que va llegando.
		void update();

		Status status() const;
		bool running() const;
		int total() const;
		int processed() const;
		int imported() const;
		int failed() const;
		// Shaders multipass: todavia no se pueden representar en una caja de un
		// solo pase. Se informan aparte para no contarlos como rotos.
		int multipass() const;
		int skipped() const;
		std::string statusLine(bool spanish) const;
		std::string lastError() const;

		// Verdadero una sola vez, cuando conviene volver a escanear la
		// biblioteca local para que aparezcan los shaders importados.
		bool takeRescan();

		std::vector<ImportResult> results() const;
		std::vector<RemoteShader> catalog() const;

		// Borra el manifiesto y los .frag importados. Devuelve cuántos archivos
		// sacó; no toca nada que no esté anotado en el manifiesto.
		int removeImported(const std::string &directory);

	private:
		struct Fetch
		{
			std::string name;
			std::string body;
			std::string error;
		};

		void workerLoop(std::string apiRoot, std::map<std::string, std::string> known,
			std::map<std::string, std::string> knownFiles, bool force);
		void importFetch(const Fetch &fetch);
		void finish();
		bool smokeRender(ofShader &shader) const;

		std::thread worker;
		std::atomic<bool> cancelFlag{false};
		std::atomic<bool> workerDone{false};

		mutable std::mutex mutex;
		std::deque<Fetch> queue;

		std::vector<RemoteShader> remote;
		std::vector<ImportResult> outcomeList;
		std::map<std::string, std::string> manifest;
		std::set<std::string> importedNames;
		std::map<std::string, std::string> importedFiles;
		std::set<std::string> usedFiles;

		std::string apiRoot;
		std::string directory;
		std::string failure;
		std::string lastCompileLog;

		Status state = Status::Idle;
		int totalCount = 0;
		int processedCount = 0;
		int importedCount = 0;
		int failedCount = 0;
		int skippedCount = 0;
		int multipassCount = 0;
		bool rescanPending = false;
		bool finalized = false;

		// FBO de 64x64 para el dibujo de prueba. Se arma una vez, en el hilo GL.
		mutable ofFbo smoke;
		mutable ofShader smokeVert;
	};

	// Ruta absoluta de la carpeta de importación dentro del árbol de datos.
	std::string importDirectory();
}
