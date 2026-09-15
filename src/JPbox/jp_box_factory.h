#pragma once
#include <string>

class JPbox;

namespace jp_box_factory
{
	// Existing stored files and interactive additions have different precedence
	// for ambiguous names (e.g. cam.xml). Preserve both until schema migration.
	enum class Context { Interactive, Stored };
	enum class Kind {
		Unknown, Shader, Image, Video, Preset, Kinect2, PointerCloud, CameraDepth,
		Camera, Spout, FrameDifference, Paint, Ndi
	};
	Kind classify(const std::string &directory, Context context);
	// Constructs but does not call setup or attach the box to any graph.
	// Caller owns the result and must clear/delete it or transfer it to a graph.
	// Unsupported types return nullptr. Feature gates are shared by all callers.
	JPbox *create(const std::string &directory, Context context);
}
