#include "jp_box_factory.h"
#include "jp_media.h"
#include "jp_box_shader.h"
#include "jp_box_image.h"
#include "jp_box_video.h"
#include "jp_box_preset.h"
#include "jp_box_kinect2.h"
#include "jp_box_pointercloud.h"
#include "jp_box_camdepth.h"
#include "jp_box_cam.h"
#include "jp_box_framedifference.h"
#include "jp_box_paint.h"
#ifdef SPOUT
#include "jp_box_spout.h"
#endif
#ifdef NDI
#include "jp_box_ndi.h"
#endif

namespace
{
	using Kind = jp_box_factory::Kind;
	bool matches(Kind kind, const std::string &directory)
	{
		auto contains = [&](const char *token) {
			return directory.find(token) != std::string::npos;
		};
		switch (kind)
		{
		case Kind::Shader: return contains(".frag");
		case Kind::Image: return jp_media::isImage(directory);
		case Kind::Video: return jp_media::isVideo(directory);
		case Kind::Preset: return ofToLower(ofFilePath::getFileExt(directory)) == "xml";
		case Kind::Kinect2: return contains("kinect2");
		case Kind::PointerCloud: return contains("pointercloud");
		case Kind::CameraDepth: return contains("camdepth");
		case Kind::Camera: return contains("cam");
#ifdef SPOUT
		case Kind::Spout: return contains("spoutReceiver");
#endif
#ifdef NDI
		case Kind::Ndi: return contains("ndiReceiver");
#endif
		case Kind::FrameDifference: return contains("framedifference");
		case Kind::Paint: return contains("paint");
		default: return false;
		}
	}
}

jp_box_factory::Kind jp_box_factory::classify(const std::string &directory,
	Context context)
{
	// Shared matching rules; only legacy precedence depends on the entry path.
	static constexpr Kind interactive[] = {
		Kind::Shader, Kind::Image, Kind::Video, Kind::Preset, Kind::Kinect2,
		Kind::PointerCloud, Kind::CameraDepth, Kind::Camera, Kind::Spout,
		Kind::FrameDifference, Kind::Paint, Kind::Ndi};
	static constexpr Kind stored[] = {
		Kind::Shader, Kind::Image, Kind::Video, Kind::Kinect2,
		Kind::PointerCloud, Kind::CameraDepth, Kind::Camera, Kind::Ndi,
		Kind::Spout, Kind::Preset, Kind::FrameDifference, Kind::Paint};
	const auto &order = context == Context::Stored ? stored : interactive;
	for (Kind kind : order)
		if (matches(kind, directory)) return kind;
	return Kind::Unknown;
}

JPbox *jp_box_factory::create(const std::string &directory, Context context)
{
	switch (classify(directory, context))
	{
	case Kind::Shader: return new JPbox_shader();
	case Kind::Image: return new JPbox_image();
	case Kind::Video: return new JPbox_video();
	case Kind::Preset: return new JPbox_preset();
	case Kind::Kinect2: return new JPbox_kinect2();
	case Kind::PointerCloud: return new JPbox_pointercloud();
	case Kind::CameraDepth: return new JPbox_camdepth();
	case Kind::Camera: return new JPbox_cam();
#ifdef SPOUT
	case Kind::Spout: return new JPbox_spout();
#endif
#ifdef NDI
	case Kind::Ndi: return new JPbox_ndi();
#endif
	case Kind::FrameDifference: return new JPbox_framedifference();
	case Kind::Paint: return new JPbox_paint();
	default: return nullptr;
	}
}
