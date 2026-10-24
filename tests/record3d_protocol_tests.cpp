#include "../src/JPbox/jp_record3d_protocol.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
	void putLittle(std::vector<uint8_t> &out, uint32_t value)
	{
		for (int i = 0; i < 4; ++i) out.push_back((value >> (i * 8)) & 255);
	}

	void putFloat(std::vector<uint8_t> &out, float value)
	{
		uint32_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		putLittle(out, bits);
	}

	// A LiDAR-shaped body: 960x720 RGB, 256x192 depth and confidence.
	std::vector<uint8_t> body(uint32_t rgbBytes, uint32_t depthBytes,
		uint32_t confidenceBytes, uint32_t miscBytes)
	{
		std::vector<uint8_t> out;
		for (uint32_t value : {960u, 720u, 256u, 192u, 256u, 192u, rgbBytes,
			depthBytes, confidenceBytes, miscBytes, 1u})
		{
			putLittle(out, value);
		}
		for (float value : {720.0f, 721.0f, 480.0f, 360.0f}) putFloat(out, value);
		for (float value : {0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.2f, 0.3f})
			putFloat(out, value);
		out.insert(out.end(), rgbBytes, 0xAA);
		out.insert(out.end(), depthBytes, 0xBB);
		out.insert(out.end(), confidenceBytes, 0xCC);
		out.insert(out.end(), miscBytes, 0xDD);
		return out;
	}

	bool near(float a, float b)
	{
		return std::fabs(a - b) < 1e-4f;
	}
}

int main()
{
	int failures = 0;
	auto check = [&](bool condition, const char *what) {
		if (!condition)
		{
			std::cerr << "FAIL: " << what << "\n";
			++failures;
		}
	};

	{
		const auto bytes = body(100, 50, 20, 4);
		jp_record3d::Message message;
		check(jp_record3d::parse(bytes.data(), bytes.size(), message),
			"well-formed body parses");
		check(message.header.depthWidth == 256 &&
			message.header.depthHeight == 192, "depth size");
		check(message.header.deviceType == jp_record3d::LIDAR, "device type");
		check(message.rgb != nullptr && message.rgb[0] == 0xAA &&
			message.rgb[99] == 0xAA, "rgb section");
		check(message.depth != nullptr && message.depth[0] == 0xBB &&
			message.depth[49] == 0xBB, "depth section");
		check(message.confidence != nullptr &&
			message.confidence[0] == 0xCC, "confidence section");
		check(message.hasConfidence(), "confidence flagged");
		check(near(message.pose.tz, 0.3f), "pose");
		check(message.depthBytes() == 256u * 192u * 4u, "decoded depth size");

		const auto k = jp_record3d::scaleToDepth(message);
		check(near(k.fx, 720.0f * 256.0f / 960.0f) &&
			near(k.fy, 721.0f * 192.0f / 720.0f) &&
			near(k.cx, 480.0f * 256.0f / 960.0f) &&
			near(k.cy, 360.0f * 192.0f / 720.0f), "intrinsics scaled to depth");
	}

	{
		// Every truncation must be refused, never read past the end.
		const auto bytes = body(100, 50, 20, 4);
		bool anyAccepted = false;
		for (size_t size = 0; size < bytes.size(); ++size)
		{
			jp_record3d::Message message;
			if (jp_record3d::parse(bytes.data(), size, message))
				anyAccepted = true;
		}
		check(!anyAccepted, "truncated bodies rejected");
	}

	{
		auto bytes = body(0, 50, 0, 0);
		jp_record3d::Message message;
		check(jp_record3d::parse(bytes.data(), bytes.size(), message),
			"rgb and confidence are optional");
		check(message.rgb == nullptr && message.confidence == nullptr &&
			!message.hasConfidence(), "absent sections are null");
	}

	{
		auto bytes = body(10, 50, 0, 0);
		bytes[0] = 0xFF; bytes[1] = 0xFF; bytes[2] = 0xFF; bytes[3] = 0x7F;
		jp_record3d::Message message;
		check(!jp_record3d::parse(bytes.data(), bytes.size(), message),
			"implausible dimension rejected");
	}

	{
		auto bytes = body(10, 0, 0, 0);
		jp_record3d::Message message;
		check(!jp_record3d::parse(bytes.data(), bytes.size(), message),
			"frame without depth rejected");
	}

	{
		// Section sizes that would wrap a 32 bit offset.
		auto bytes = body(10, 50, 0, 0);
		const uint32_t huge = 0xFFFFFFF0u;
		for (int i = 0; i < 4; ++i) bytes[24 + i] = (huge >> (i * 8)) & 255;
		jp_record3d::Message message;
		check(!jp_record3d::parse(bytes.data(), bytes.size(), message),
			"oversized section rejected");
	}

	{
		uint8_t header[16] = {0};
		header[14] = 0x01;  // 256 bytes, big-endian
		check(jp_record3d::bodySize(header) == 256, "PeerTalk body size");
		header[14] = 0;
		header[15] = 8;
		check(jp_record3d::bodySize(header) == 0, "body smaller than header");
		header[12] = 0x7F;
		check(jp_record3d::bodySize(header) == 0, "body larger than limit");
	}

	if (failures)
	{
		std::cerr << failures << " Record3D protocol test(s) failed\n";
		return 1;
	}
	std::cout << "Record3D protocol tests passed\n";
	return 0;
}
