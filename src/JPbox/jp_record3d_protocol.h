#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Wire format of the Record3D iOS app's USB stream (app 1.10 or newer), as
// read by https://github.com/marek-simonik/record3d. That library is not
// linked: its reader thread is detached and blocks in a recv that nothing can
// interrupt, so a box being deleted would free the stream under it. Guipper
// reads the same bytes on its own thread instead.
//
// Every message is a 16 byte PeerTalk header whose last word is the body size
// in network order, then a body laid out as:
//   Header | Intrinsics | Pose | RGB JPEG | depth LZFSE | confidence LZFSE | misc
// The fixed-size parts are little-endian, as the iPhone writes them.
//
// Kept free of openFrameworks and of the compression libraries so tests/ can
// exercise the bounds checks without a device.
namespace jp_record3d
{
	constexpr uint16_t kDevicePort = 1337;
	constexpr size_t kPeerTalkHeaderSize = 16;
	// A LiDAR frame is a few hundred KB. Anything this large is a desynced
	// stream, not a picture, and must not become an allocation.
	constexpr uint32_t kMaxBodySize = 64u * 1024u * 1024u;
	constexpr uint32_t kMaxDimension = 8192;

	enum DeviceType : uint32_t
	{
		TRUE_DEPTH = 0,
		LIDAR = 1
	};

	struct Header
	{
		uint32_t rgbWidth = 0;
		uint32_t rgbHeight = 0;
		uint32_t depthWidth = 0;
		uint32_t depthHeight = 0;
		uint32_t confidenceWidth = 0;
		uint32_t confidenceHeight = 0;
		uint32_t rgbSize = 0;
		uint32_t depthSize = 0;
		uint32_t confidenceSize = 0;
		uint32_t miscSize = 0;
		uint32_t deviceType = 0;
	};

	// Pinhole coefficients of the RGB image, in RGB pixels.
	struct Intrinsics
	{
		float fx = 0.0f;
		float fy = 0.0f;
		float cx = 0.0f;
		float cy = 0.0f;
	};

	struct Pose
	{
		float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
		float tx = 0.0f, ty = 0.0f, tz = 0.0f;
	};

	constexpr size_t kHeaderBytes = 11 * 4;
	constexpr size_t kIntrinsicsBytes = 4 * 4;
	constexpr size_t kPoseBytes = 7 * 4;

	// Views into the body. Valid only while the body buffer is.
	struct Message
	{
		Header header;
		Intrinsics intrinsics;
		Pose pose;
		const uint8_t *rgb = nullptr;
		const uint8_t *depth = nullptr;
		const uint8_t *confidence = nullptr;

		size_t depthBytes() const
		{
			return (size_t)header.depthWidth * header.depthHeight * sizeof(float);
		}
		size_t confidenceBytes() const
		{
			return (size_t)header.confidenceWidth * header.confidenceHeight;
		}
		bool hasConfidence() const
		{
			return header.confidenceSize > 0 && confidenceBytes() > 0;
		}
	};

	inline uint32_t readLittle(const uint8_t *bytes)
	{
		return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
			((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
	}

	inline float readFloat(const uint8_t *bytes)
	{
		const uint32_t bits = readLittle(bytes);
		float value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	// Body size from a PeerTalk header, or 0 when it cannot be a frame.
	inline uint32_t bodySize(const uint8_t *peerTalk)
	{
		const uint8_t *word = peerTalk + 12;
		const uint32_t size = ((uint32_t)word[0] << 24) |
			((uint32_t)word[1] << 16) | ((uint32_t)word[2] << 8) |
			(uint32_t)word[3];
		if (size < kHeaderBytes + kIntrinsicsBytes + kPoseBytes ||
			size > kMaxBodySize)
		{
			return 0;
		}
		return size;
	}

	// False when a section overruns the body or a dimension is implausible -
	// the caller drops the frame rather than decoding garbage.
	inline bool parse(const uint8_t *body, size_t size, Message &out)
	{
		size_t offset = 0;
		auto take = [&](size_t bytes) -> const uint8_t * {
			if (bytes > size - offset) return nullptr;
			const uint8_t *at = body + offset;
			offset += bytes;
			return at;
		};

		const uint8_t *header = take(kHeaderBytes);
		if (header == nullptr) return false;
		uint32_t fields[11];
		for (int i = 0; i < 11; ++i) fields[i] = readLittle(header + i * 4);
		Header &h = out.header;
		h.rgbWidth = fields[0];
		h.rgbHeight = fields[1];
		h.depthWidth = fields[2];
		h.depthHeight = fields[3];
		h.confidenceWidth = fields[4];
		h.confidenceHeight = fields[5];
		h.rgbSize = fields[6];
		h.depthSize = fields[7];
		h.confidenceSize = fields[8];
		h.miscSize = fields[9];
		h.deviceType = fields[10];

		if (h.depthWidth == 0 || h.depthHeight == 0 ||
			h.depthWidth > kMaxDimension || h.depthHeight > kMaxDimension ||
			h.rgbWidth > kMaxDimension || h.rgbHeight > kMaxDimension ||
			h.confidenceWidth > kMaxDimension ||
			h.confidenceHeight > kMaxDimension || h.depthSize == 0)
		{
			return false;
		}

		const uint8_t *intrinsics = take(kIntrinsicsBytes);
		const uint8_t *pose = take(kPoseBytes);
		if (intrinsics == nullptr || pose == nullptr) return false;
		out.intrinsics.fx = readFloat(intrinsics);
		out.intrinsics.fy = readFloat(intrinsics + 4);
		out.intrinsics.cx = readFloat(intrinsics + 8);
		out.intrinsics.cy = readFloat(intrinsics + 12);
		float poseValues[7];
		for (int i = 0; i < 7; ++i) poseValues[i] = readFloat(pose + i * 4);
		out.pose = {poseValues[0], poseValues[1], poseValues[2], poseValues[3],
			poseValues[4], poseValues[5], poseValues[6]};

		out.rgb = take(h.rgbSize);
		out.depth = take(h.depthSize);
		out.confidence = take(h.confidenceSize);
		if (out.rgb == nullptr || out.depth == nullptr ||
			out.confidence == nullptr || take(h.miscSize) == nullptr)
		{
			return false;
		}
		if (h.rgbSize == 0) out.rgb = nullptr;
		if (h.confidenceSize == 0) out.confidence = nullptr;
		return true;
	}

	// The stream only carries RGB intrinsics; depth covers the same field of
	// view at a lower resolution, so they scale with the image.
	inline Intrinsics scaleToDepth(const Message &message)
	{
		const Header &h = message.header;
		if (h.rgbWidth == 0 || h.rgbHeight == 0) return Intrinsics();
		const float sx = (float)h.depthWidth / (float)h.rgbWidth;
		const float sy = (float)h.depthHeight / (float)h.rgbHeight;
		return {message.intrinsics.fx * sx, message.intrinsics.fy * sy,
			message.intrinsics.cx * sx, message.intrinsics.cy * sy};
	}
}
