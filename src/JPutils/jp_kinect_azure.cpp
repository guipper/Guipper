#include "jp_kinect_azure.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace jp_kinect_azure
{
	namespace
	{
#ifdef _WIN32
		// --- Structs de k4a, copiadas del header 1.4.1 -----------------------
		// Se declaran a mano para no exigir la SDK al compilar. Los asserts
		// fijan el layout verificado contra la SDK instalada.
		struct Extrinsics
		{
			float rotation[9];
			float translation[3];
		};
		struct Params
		{
			float cx, cy, fx, fy;
			float k1, k2, k3, k4, k5, k6;
			float codx, cody;
			float p2, p1;
			float metric_radius;
		};
		struct IntrinsicsBlock
		{
			int32_t type;
			uint32_t parameter_count;
			Params parameters;
		};
		struct CameraCalibration
		{
			Extrinsics extrinsics;
			IntrinsicsBlock intrinsics;
			int32_t resolution_width;
			int32_t resolution_height;
			float metric_radius;
		};
		struct Calibration
		{
			CameraCalibration depth_camera_calibration;
			CameraCalibration color_camera_calibration;
			Extrinsics extrinsics[4][4];
			// Los dos enums del final son parte del struct: sin ellos el tamano
			// queda 8 bytes corto y la SDK escribiria fuera del buffer.
			int32_t depth_mode;
			int32_t color_resolution;
		};
		struct DeviceConfig
		{
			int32_t color_format;
			int32_t color_resolution;
			int32_t depth_mode;
			int32_t camera_fps;
			uint8_t synchronized_images_only;
			int32_t depth_delay_off_color_usec;
			int32_t wired_sync_mode;
			uint32_t subordinate_delay_off_master_usec;
			uint8_t disable_streaming_indicator;
		};
		struct Float2
		{
			float xy[2];
		};

		static_assert(sizeof(Extrinsics) == 48, "ABI k4a: extrinsics");
		static_assert(sizeof(Params) == 60, "ABI k4a: parametros de intrinsics");
		static_assert(sizeof(IntrinsicsBlock) == 68, "ABI k4a: intrinsics");
		static_assert(sizeof(CameraCalibration) == 128, "ABI k4a: camara");
		static_assert(sizeof(Calibration) == 1032, "ABI k4a: calibracion");
		static_assert(sizeof(DeviceConfig) == 36, "ABI k4a: configuracion");
		static_assert(offsetof(DeviceConfig, synchronized_images_only) == 16, "ABI k4a: sync");
		static_assert(offsetof(DeviceConfig, depth_delay_off_color_usec) == 20, "ABI k4a: delay");
		static_assert(offsetof(DeviceConfig, wired_sync_mode) == 24, "ABI k4a: wired");
		static_assert(offsetof(DeviceConfig, subordinate_delay_off_master_usec) == 28, "ABI k4a: subdelay");
		static_assert(offsetof(DeviceConfig, disable_streaming_indicator) == 32, "ABI k4a: indicator");
		static_assert(offsetof(Calibration, color_camera_calibration) == 128, "ABI k4a: color");
		static_assert(offsetof(Calibration, extrinsics) == 256, "ABI k4a: extrinsics tabla");
		static_assert(offsetof(CameraCalibration, intrinsics) == 48, "ABI k4a: intr offset");
		static_assert(offsetof(CameraCalibration, resolution_width) == 116, "ABI k4a: width");
		static_assert(offsetof(IntrinsicsBlock, parameters) == 8, "ABI k4a: params offset");
		static_assert(offsetof(Params, fx) == 8, "ABI k4a: fx");

		// Valores de los enums (k4atypes.h 1.4.1).
		constexpr int32_t kFormatBgra32 = 3;
		constexpr int32_t kRes1080p = 2;
		constexpr int32_t kDepthNfovUnbinned = 2;
		constexpr int32_t kFps30 = 2;
		constexpr int32_t kWiredStandalone = 0;
		constexpr int32_t kCalibrationDepth = 0;
		constexpr int32_t kCalibrationColor = 1;
		constexpr int32_t kWaitSucceeded = 0;
		constexpr int32_t kResultSucceeded = 0;

		// El IR de Azure sale mucho mas bajo que el de Kinect v2 (p95 medido
		// ~1500 contra el rango 0..65535 que asume el mapeo gris del nodo).
		// Se normaliza aca para que el mismo IR gain/gamma sirva en las dos
		// camaras; el factor sale de esa medicion con la camara real.
		constexpr float kInfraredScale = 32.0f;

		// --- Carga dinamica de k4a.dll --------------------------------------
		struct Api
		{
			HMODULE module = nullptr;
			std::string directory;

			uint32_t (*device_get_installed_count)() = nullptr;
			int32_t (*device_open)(uint32_t, void **) = nullptr;
			void (*device_close)(void *) = nullptr;
			int32_t (*device_start_cameras)(void *, const DeviceConfig *) = nullptr;
			void (*device_stop_cameras)(void *) = nullptr;
			int32_t (*device_get_capture)(void *, void **, int32_t) = nullptr;
			void (*capture_release)(void *) = nullptr;
			void *(*capture_get_color_image)(void *) = nullptr;
			void *(*capture_get_depth_image)(void *) = nullptr;
			void *(*capture_get_ir_image)(void *) = nullptr;
			void (*image_release)(void *) = nullptr;
			uint8_t *(*image_get_buffer)(void *) = nullptr;
			int (*image_get_width_pixels)(void *) = nullptr;
			int (*image_get_height_pixels)(void *) = nullptr;
			int (*image_get_stride_bytes)(void *) = nullptr;
			uint64_t (*image_get_device_timestamp_usec)(void *) = nullptr;
			int32_t (*device_get_calibration)(void *, int32_t, int32_t, Calibration *) = nullptr;
			int32_t (*calibration_2d_to_2d)(const Calibration *, const Float2 *, float, int32_t,
				int32_t, Float2 *, int *) = nullptr;
		};

		std::wstring widen(const std::string &text)
		{
			if (text.empty()) return {};
			const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
			std::wstring wide(size, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), size);
			return wide;
		}

		std::string exeDirectory()
		{
			wchar_t buffer[MAX_PATH * 2] = {0};
			const DWORD length = GetModuleFileNameW(nullptr, buffer, (DWORD)std::size(buffer));
			if (length == 0) return {};
			return std::filesystem::path(buffer).parent_path().string();
		}

		// Busca la carpeta bin de cualquier "Azure Kinect SDK*" instalada.
		std::string findSdkBin()
		{
			std::vector<std::string> roots;
			if (const char *env = std::getenv("K4A_SDK_BIN")) roots.emplace_back(env);
			if (const char *pf = std::getenv("ProgramFiles"))
			{
				std::error_code error;
				for (const auto &entry : std::filesystem::directory_iterator(pf, error))
				{
					if (error) break;
					if (!entry.is_directory()) continue;
					const std::string name = entry.path().filename().string();
					if (name.rfind("Azure Kinect SDK", 0) != 0) continue;
					roots.push_back(entry.path().string());
				}
			}
			for (const auto &root : roots)
			{
				const std::filesystem::path bin = std::filesystem::path(root) /
					"sdk" / "windows-desktop" / "amd64" / "release" / "bin";
				std::error_code error;
				if (std::filesystem::is_regular_file(bin / "k4a.dll", error))
					return bin.string();
				// K4A_SDK_BIN puede apuntar directo a la carpeta de las DLLs.
				if (std::filesystem::is_regular_file(std::filesystem::path(root) / "k4a.dll", error))
					return root;
			}
			return {};
		}

		Api &api()
		{
			static Api instance;
			static std::once_flag once;
			std::call_once(once, [&] {
				// Primero al lado del ejecutable: es lo que hace una instalacion
				// que copia las DLLs. Despues, la SDK instalada.
				std::string dir = exeDirectory();
				HMODULE module = nullptr;
				if (!dir.empty())
					module = LoadLibraryW((widen(dir) + L"\\k4a.dll").c_str());
				if (!module)
				{
					const std::string bin = findSdkBin();
					if (!bin.empty())
					{
						// k4a carga depthengine_2_0.dll por nombre en runtime:
						// la carpeta de la SDK tiene que quedar en la busqueda.
						SetDllDirectoryW(widen(bin).c_str());
						module = LoadLibraryW((widen(bin) + L"\\k4a.dll").c_str());
						if (module) dir = bin;
						else SetDllDirectoryW(nullptr);
					}
				}
				if (!module) return;
				instance.module = module;
				instance.directory = dir;
				auto bind = [&](const char *name) { return GetProcAddress(module, name); };
				instance.device_get_installed_count =
					(decltype(instance.device_get_installed_count))bind("k4a_device_get_installed_count");
				instance.device_open = (decltype(instance.device_open))bind("k4a_device_open");
				instance.device_close = (decltype(instance.device_close))bind("k4a_device_close");
				instance.device_start_cameras = (decltype(instance.device_start_cameras))bind("k4a_device_start_cameras");
				instance.device_stop_cameras = (decltype(instance.device_stop_cameras))bind("k4a_device_stop_cameras");
				instance.device_get_capture = (decltype(instance.device_get_capture))bind("k4a_device_get_capture");
				instance.capture_release = (decltype(instance.capture_release))bind("k4a_capture_release");
				instance.capture_get_color_image = (decltype(instance.capture_get_color_image))bind("k4a_capture_get_color_image");
				instance.capture_get_depth_image = (decltype(instance.capture_get_depth_image))bind("k4a_capture_get_depth_image");
				instance.capture_get_ir_image = (decltype(instance.capture_get_ir_image))bind("k4a_capture_get_ir_image");
				instance.image_release = (decltype(instance.image_release))bind("k4a_image_release");
				instance.image_get_buffer = (decltype(instance.image_get_buffer))bind("k4a_image_get_buffer");
				instance.image_get_width_pixels = (decltype(instance.image_get_width_pixels))bind("k4a_image_get_width_pixels");
				instance.image_get_height_pixels = (decltype(instance.image_get_height_pixels))bind("k4a_image_get_height_pixels");
				instance.image_get_stride_bytes = (decltype(instance.image_get_stride_bytes))bind("k4a_image_get_stride_bytes");
				instance.image_get_device_timestamp_usec =
					(decltype(instance.image_get_device_timestamp_usec))bind("k4a_image_get_device_timestamp_usec");
				instance.device_get_calibration = (decltype(instance.device_get_calibration))bind("k4a_device_get_calibration");
				instance.calibration_2d_to_2d = (decltype(instance.calibration_2d_to_2d))bind("k4a_calibration_2d_to_2d");
				if (!instance.device_get_installed_count || !instance.device_open || !instance.device_start_cameras ||
					!instance.device_get_capture || !instance.capture_get_depth_image || !instance.image_get_buffer)
				{
					// DLL de otra version sin las funciones minimas.
					instance = Api();
				}
			});
			return instance;
		}
#else
		struct Api
		{
			HMODULE module = nullptr;
			std::string directory;
		};
		Api &api()
		{
			static Api empty;
			return empty;
		}
#endif

		// Escala uniforme que mete la grilla nativa de Azure en 512x424 sin
		// deformar: se ajusta la altura y quedan barras a los costados.
		struct Mapping
		{
			float scale = 1.0f;
			int offsetX = 0;
			int offsetY = 0;
			int sourceWidth = kDepthWidth;
			int sourceHeight = kDepthHeight;
		};

		Mapping mappingFor(int width, int height)
		{
			Mapping map;
			map.sourceWidth = std::max(1, width);
			map.sourceHeight = std::max(1, height);
			map.scale = std::min(float(kDepthWidth) / float(map.sourceWidth),
				float(kDepthHeight) / float(map.sourceHeight));
			const int scaledWidth = std::max(1, (int)std::lround(map.sourceWidth * map.scale));
			const int scaledHeight = std::max(1, (int)std::lround(map.sourceHeight * map.scale));
			map.offsetX = (kDepthWidth - scaledWidth) / 2;
			map.offsetY = (kDepthHeight - scaledHeight) / 2;
			return map;
		}
	}

	bool available()
	{
#ifdef _WIN32
		return api().module != nullptr;
#else
		return false;
#endif
	}

	int deviceCount()
	{
#ifdef _WIN32
		const Api &context = api();
		if (!context.module || !context.device_get_installed_count) return -1;
		return (int)context.device_get_installed_count();
#else
		return -1;
#endif
	}

	std::string runtimePath()
	{
		return api().directory;
	}

#ifdef _WIN32
	Capture::~Capture()
	{
		close();
	}

	void Capture::close()
	{
		const Api &context = api();
		if (device && context.module)
		{
			if (context.device_stop_cameras) context.device_stop_cameras(device);
			if (context.device_close) context.device_close(device);
		}
		device = nullptr;
		delete[] static_cast<uint8_t *>(calibration);
		calibration = nullptr;
		colorToDepth.clear();
		colorToDepthReady = false;
	}

	bool Capture::open()
	{
		close();
		const Api &context = api();
		if (!context.module)
		{
			status = "INSTALAR AZURE KINECT SDK";
			return false;
		}
		const int installed = (int)context.device_get_installed_count();
		if (installed <= 0)
		{
			status = "AZURE KINECT NOT FOUND";
			return false;
		}
		void *handle = nullptr;
		if (context.device_open(0, &handle) != kResultSucceeded || !handle)
		{
			status = "AZURE OPEN FAILED / USB EN USO";
			return false;
		}
		DeviceConfig config{};
		config.color_format = kFormatBgra32;
		config.color_resolution = kRes1080p;
		config.depth_mode = kDepthNfovUnbinned;
		config.camera_fps = kFps30;
		config.synchronized_images_only = 0; // el color puede tardar mas que la profundidad
		config.depth_delay_off_color_usec = 0;
		config.wired_sync_mode = kWiredStandalone;
		config.subordinate_delay_off_master_usec = 0;
		config.disable_streaming_indicator = 0;
		if (context.device_start_cameras(handle, &config) != kResultSucceeded)
		{
			context.device_close(handle);
			status = "AZURE STREAM START FAILED";
			return false;
		}
		device = handle;

		calibration = new (std::nothrow) uint8_t[sizeof(Calibration)];
		if (!calibration || !context.device_get_calibration ||
			context.device_get_calibration(device, kDepthNfovUnbinned, kRes1080p,
				reinterpret_cast<Calibration *>(calibration)) != kResultSucceeded)
		{
			status = "AZURE CALIBRATION FAILED";
			close();
			return false;
		}
		const auto *cal = reinterpret_cast<const Calibration *>(calibration);
		const Mapping map = mappingFor(cal->depth_camera_calibration.resolution_width,
			cal->depth_camera_calibration.resolution_height);
		const Params &params = cal->depth_camera_calibration.intrinsics.parameters;
		intrinsics.FocalLengthX = params.fx * map.scale;
		intrinsics.FocalLengthY = params.fy * map.scale;
		intrinsics.PrincipalPointX = params.cx * map.scale + float(map.offsetX);
		intrinsics.PrincipalPointY = params.cy * map.scale + float(map.offsetY);

		depth.assign(size_t(kDepthWidth) * kDepthHeight, 0);
		infrared.assign(size_t(kDepthWidth) * kDepthHeight, 0);
		color.assign(size_t(kColorWidth) * kColorHeight * 4, 0);
		status = "STREAMING";
		return true;
	}

	bool Capture::read()
	{
		newColor = false;
		const Api &context = api();
		if (!device || !context.module) return false;
		void *capture = nullptr;
		if (context.device_get_capture(device, &capture, 500) != kWaitSucceeded || !capture)
		{
			status = "AZURE WAITING FOR FRAMES";
			return false;
		}
		bool gotDepth = false;
		void *depthImage = context.capture_get_depth_image(capture);
		void *irImage = context.capture_get_ir_image ? context.capture_get_ir_image(capture) : nullptr;
		void *colorImage = context.capture_get_color_image(capture);
		const auto *cal = reinterpret_cast<const Calibration *>(calibration);
		if (depthImage && cal)
		{
			const int width = context.image_get_width_pixels(depthImage);
			const int height = context.image_get_height_pixels(depthImage);
			const int stride = context.image_get_stride_bytes(depthImage);
			const uint8_t *buffer = context.image_get_buffer(depthImage);
			const Mapping map = mappingFor(width, height);
			if (buffer && width > 0 && height > 0 && stride >= width * 2)
			{
				for (int y = 0; y < kDepthHeight; ++y)
				{
					const int sourceY = (int)std::lround((y - map.offsetY) / map.scale);
					uint16_t *row = depth.data() + size_t(y) * kDepthWidth;
					if (sourceY < 0 || sourceY >= height)
					{
						std::fill(row, row + kDepthWidth, uint16_t(0));
						continue;
					}
					const uint8_t *sourceRow = buffer + size_t(sourceY) * stride;
					for (int x = 0; x < kDepthWidth; ++x)
					{
						const int sourceX = (int)std::lround((x - map.offsetX) / map.scale);
						row[x] = (sourceX >= 0 && sourceX < width)
							? reinterpret_cast<const uint16_t *>(sourceRow)[sourceX] : uint16_t(0);
					}
				}
				depthTime = int64_t(context.image_get_device_timestamp_usec(depthImage)) * 10;
				gotDepth = true;
			}
		}
		if (irImage && cal)
		{
			const int width = context.image_get_width_pixels(irImage);
			const int height = context.image_get_height_pixels(irImage);
			const int stride = context.image_get_stride_bytes(irImage);
			const uint8_t *buffer = context.image_get_buffer(irImage);
			const Mapping map = mappingFor(width, height);
			if (buffer && width > 0 && height > 0 && stride >= width * 2)
			{
				for (int y = 0; y < kDepthHeight; ++y)
				{
					const int sourceY = (int)std::lround((y - map.offsetY) / map.scale);
					uint16_t *row = infrared.data() + size_t(y) * kDepthWidth;
					if (sourceY < 0 || sourceY >= height)
					{
						std::fill(row, row + kDepthWidth, uint16_t(0));
						continue;
					}
					const uint8_t *sourceRow = buffer + size_t(sourceY) * stride;
					for (int x = 0; x < kDepthWidth; ++x)
					{
						const int sourceX = (int)std::lround((x - map.offsetX) / map.scale);
						row[x] = (sourceX >= 0 && sourceX < width)
							? uint16_t(std::min(65535.0f,
								reinterpret_cast<const uint16_t *>(sourceRow)[sourceX] * kInfraredScale))
							: uint16_t(0);
					}
				}
			}
		}
		if (colorImage)
		{
			const int width = context.image_get_width_pixels(colorImage);
			const int height = context.image_get_height_pixels(colorImage);
			const int stride = context.image_get_stride_bytes(colorImage);
			const uint8_t *buffer = context.image_get_buffer(colorImage);
			if (buffer && width == kColorWidth && height == kColorHeight && stride >= width * 4)
			{
				// k4a entrega BGRA32; Guipper espera RGBA.
				for (int y = 0; y < kColorHeight; ++y)
				{
					const uint8_t *source = buffer + size_t(y) * stride;
					unsigned char *target = color.data() + size_t(y) * kColorWidth * 4;
					for (int x = 0; x < kColorWidth; ++x)
					{
						target[x * 4 + 0] = source[x * 4 + 2];
						target[x * 4 + 1] = source[x * 4 + 1];
						target[x * 4 + 2] = source[x * 4 + 0];
						target[x * 4 + 3] = 255;
					}
				}
				colorTime = int64_t(context.image_get_device_timestamp_usec(colorImage)) * 10;
				newColor = true;
			}
		}
		if (colorImage && context.image_release) context.image_release(colorImage);
		if (irImage && context.image_release) context.image_release(irImage);
		if (depthImage && context.image_release) context.image_release(depthImage);
		context.capture_release(capture);
		status = gotDepth ? "STREAMING" : "AZURE WAITING FOR FRAMES";
		return gotDepth;
	}

	std::vector<float> Capture::colorDepth()
	{
		// Detectores de manos necesitan la profundidad por pixel de color, y el
		// mapeo depende solo de la calibracion: se calcula una vez a media
		// resolucion y se expande, en vez de 2 millones de conversiones por frame.
		if (!newColor || !calibration) return {};
		const Api &context = api();
		const auto *cal = reinterpret_cast<const Calibration *>(calibration);
		constexpr int kLutWidth = kColorWidth / 2;
		constexpr int kLutHeight = kColorHeight / 2;
		if (!colorToDepthReady && !calibrating && context.calibration_2d_to_2d)
		{
			calibrating = true;
			const CameraCalibration &depthCamera = cal->depth_camera_calibration;
			const Mapping map = mappingFor(depthCamera.resolution_width, depthCamera.resolution_height);
			colorToDepth.assign(size_t(kLutWidth) * kLutHeight, -1);
			for (int y = 0; y < kLutHeight; ++y)
			{
				for (int x = 0; x < kLutWidth; ++x)
				{
					Float2 source{};
					source.xy[0] = float(x * 2) + 0.5f;
					source.xy[1] = float(y * 2) + 0.5f;
					Float2 target{};
					int valid = 0;
					if (context.calibration_2d_to_2d(cal, &source, 1000.0f, kCalibrationColor,
							kCalibrationDepth, &target, &valid) != kResultSucceeded || !valid)
						continue;
					const int depthX = (int)std::lround(target.xy[0] * map.scale) + map.offsetX;
					const int depthY = (int)std::lround(target.xy[1] * map.scale) + map.offsetY;
					if (depthX < 0 || depthX >= kDepthWidth || depthY < 0 || depthY >= kDepthHeight) continue;
					colorToDepth[size_t(y) * kLutWidth + x] = depthY * kDepthWidth + depthX;
				}
			}
			colorToDepthReady = true;
			calibrating = false;
		}
		if (!colorToDepthReady) return {};
		std::vector<float> result(size_t(kColorWidth) * kColorHeight, 0.0f);
		for (int y = 0; y < kColorHeight; ++y)
		{
			const int lutRow = (y / 2) * kLutWidth;
			float *target = result.data() + size_t(y) * kColorWidth;
			for (int x = 0; x < kColorWidth; ++x)
			{
				const int32_t index = colorToDepth[lutRow + x / 2];
				if (index >= 0) target[x] = float(depth[size_t(index)]);
			}
		}
		return result;
	}
#else
	Capture::~Capture() {}
	void Capture::close() {}
	bool Capture::open()
	{
		status = "AZURE SOLO EN WINDOWS";
		return false;
	}
	bool Capture::read() { return false; }
	std::vector<float> Capture::colorDepth() { return {}; }
#endif
}
