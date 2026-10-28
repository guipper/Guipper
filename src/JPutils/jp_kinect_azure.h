#pragma once

// Azure Kinect DK como segunda fuente de captura.
//
// La SDK oficial NO es una dependencia de compilacion: k4a.dll se busca en
// runtime (junto al .exe, en la carpeta de la SDK o en el PATH) y todas las
// funciones se resuelven con GetProcAddress. Asi Guipper sigue compilando en
// cualquier maquina y, si la SDK no esta, la fuente simplemente informa el
// motivo en `status` sin afectar al resto de la app.
//
// Las structs de k4a se declaran a mano porque no podemos incluir sus headers.
// Los tamanos y offsets fueron verificados contra la SDK 1.4.1 instalada
// (config 36 bytes, calibracion 1032, intrinsics 68, parametros 60) y estan
// fijados con asserts de compilacion mas abajo.
#include <cstdint>
#include <string>
#include <vector>

namespace jp_kinect_azure
{
	// La app trabaja con la grilla de Kinect v2; Azure se remuestrea a esta.
	constexpr int kDepthWidth = 512;
	constexpr int kDepthHeight = 424;
	constexpr int kColorWidth = 1920;
	constexpr int kColorHeight = 1080;

	struct Intrinsics
	{
		// Mismos nombres que CameraIntrinsics de Kinect v2: el publicador de
		// frames es el mismo codigo para las dos fuentes.
		float FocalLengthX = 0.0f;
		float FocalLengthY = 0.0f;
		float PrincipalPointX = 0.0f;
		float PrincipalPointY = 0.0f;
	};

	// k4a.dll encontrada y con ABI compatible.
	bool available();
	// -1: sin SDK · 0: SDK pero ningun dispositivo conectado · >0: dispositivos.
	int deviceCount();
	// Ruta de la carpeta de la SDK que se uso, para diagnostico.
	std::string runtimePath();

	class Capture
	{
	public:
		const char *status = "CONNECTING";
		Intrinsics intrinsics{};
		std::vector<uint16_t> depth;
		std::vector<uint16_t> infrared;
		std::vector<unsigned char> color;
		int64_t depthTime = 0;
		int64_t colorTime = 0;
		bool newColor = false;

		Capture() = default;
		Capture(const Capture &) = delete;
		Capture &operator=(const Capture &) = delete;
		~Capture();

		bool open();
		// Un frame nuevo de profundidad/IR y, cuando llega, uno de color.
		bool read();
		// Profundidad en milimetros por pixel de color (0 = sin dato).
		std::vector<float> colorDepth();

	private:
		void close();

		void *device = nullptr;   // k4a_device_t
		void *calibration = nullptr; // k4a_calibration_t, alineada
		std::vector<int32_t> colorToDepth;
		bool colorToDepthReady = false;
		bool calibrating = false;
	};
}
