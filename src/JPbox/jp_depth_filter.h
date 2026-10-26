#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Cleans a metric depth stream (millimetres, 0 = no reading) before it is
// drawn or unprojected. Shared by the iPhone LiDAR box and PointerCloud, and
// kept free of openFrameworks so tests/ can drive it.
//
// Two independent steps:
//  - Temporal smoothing. Each pixel eases towards the new reading, but only
//    while the change is small enough to be sensor noise. A larger jump is a
//    real edge or a person moving and is taken at once, so smoothing never
//    leaves a ghost trailing behind a body.
//  - Hole filling. A pixel that drops out keeps its last value for a few
//    frames (LiDAR flickers at range), and a gap surrounded by one continuous
//    surface takes the mean of its neighbours. A gap on a silhouette edge,
//    whose neighbours disagree, stays empty rather than bridging foreground
//    and background.
namespace jp_depth_filter
{
	struct Settings
	{
		// 0 off .. 1 heavy.
		float smooth = 0.0f;
		bool fillHoles = false;
	};

	// Below this a difference is treated as noise. Grows with distance,
	// because so does the sensor's error.
	inline float tolerance(float mm)
	{
		return 25.0f + 0.03f * mm;
	}

	class Filter
	{
	public:
		static constexpr uint8_t kHoldFrames = 3;

		void reset()
		{
			state.clear();
			held.clear();
			width = height = 0;
		}

		// `in` holds width*height millimetres. `out` is resized to match.
		void apply(const float *in, int w, int h, const Settings &settings,
			std::vector<float> &out)
		{
			const size_t pixels = (size_t)std::max(0, w) * std::max(0, h);
			out.assign(in, in + pixels);
			const float smooth = std::min(1.0f, std::max(0.0f, settings.smooth));
			if (smooth <= 0.0f && !settings.fillHoles)
			{
				reset();
				return;
			}
			if (w != width || h != height || state.size() != pixels)
			{
				width = w;
				height = h;
				state.assign(pixels, 0.0f);
				held.assign(pixels, 0);
			}

			const float alpha = 1.0f - 0.9f * smooth;
			for (size_t i = 0; i < pixels; ++i)
			{
				const float current = valid(in[i]) ? in[i] : 0.0f;
				const float previous = state[i];
				float next = current;
				if (current > 0.0f)
				{
					held[i] = 0;
					if (previous > 0.0f &&
						std::fabs(current - previous) < tolerance(previous))
					{
						next = previous + alpha * (current - previous);
					}
				}
				else if (settings.fillHoles && previous > 0.0f &&
					held[i] < kHoldFrames)
				{
					++held[i];
					next = previous;
				}
				state[i] = next;
				out[i] = next;
			}

			// Spatial pass reads the temporal result and writes `out`, so a
			// filled pixel never seeds its neighbour in the same frame - holes
			// cannot grow into a smear across an edge.
			if (!settings.fillHoles) return;
			for (int y = 0; y < h; ++y)
			{
				for (int x = 0; x < w; ++x)
				{
					const size_t i = (size_t)y * w + x;
					if (state[i] > 0.0f) continue;
					float sum = 0.0f, lo = 1e9f, hi = 0.0f;
					int count = 0;
					for (int dy = -1; dy <= 1; ++dy)
					{
						for (int dx = -1; dx <= 1; ++dx)
						{
							const int nx = x + dx, ny = y + dy;
							if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 ||
								nx >= w || ny >= h)
							{
								continue;
							}
							const float d = state[(size_t)ny * w + nx];
							if (d <= 0.0f) continue;
							sum += d;
							lo = std::min(lo, d);
							hi = std::max(hi, d);
							++count;
						}
					}
					if (count >= 5 && hi - lo < tolerance(lo)) out[i] = sum / count;
				}
			}
		}

	private:
		static bool valid(float mm)
		{
			return std::isfinite(mm) && mm > 0.0f;
		}

		std::vector<float> state;
		std::vector<uint8_t> held;
		int width = 0;
		int height = 0;
	};
}
