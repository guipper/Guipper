#include "../src/JPbox/jp_depth_filter.h"

#include <cmath>
#include <iostream>
#include <vector>

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
	using jp_depth_filter::Filter;
	using jp_depth_filter::Settings;
	constexpr int W = 8, H = 8;
	std::vector<float> frame(W * H, 3000.0f), out;

	{
		// Off is an exact copy, including holes and NaN-free zeros.
		Filter filter;
		frame[10] = 0.0f;
		filter.apply(frame.data(), W, H, Settings(), out);
		check(out == frame, "disabled filter is a copy");
		frame[10] = 3000.0f;
	}

	{
		// Noise of +-20 mm around 3 m shrinks.
		Filter filter;
		Settings settings;
		settings.smooth = 0.8f;
		float worst = 0.0f;
		for (int t = 0; t < 60; ++t)
		{
			std::vector<float> noisy(frame);
			for (auto &d : noisy) d += (t % 2 ? 20.0f : -20.0f);
			filter.apply(noisy.data(), W, H, settings, out);
			if (t > 20) worst = std::max(worst, std::fabs(out[0] - 3000.0f));
		}
		check(worst < 8.0f, "noise attenuated");
	}

	{
		// A person stepping in is taken at once: no ghost.
		Filter filter;
		Settings settings;
		settings.smooth = 1.0f;
		for (int t = 0; t < 10; ++t) filter.apply(frame.data(), W, H, settings, out);
		std::vector<float> near(frame);
		near[0] = 1500.0f;
		filter.apply(near.data(), W, H, settings, out);
		check(out[0] == 1500.0f, "large change passes through");
	}

	{
		// Dropouts are held briefly, then released.
		Filter filter;
		Settings settings;
		settings.fillHoles = true;
		filter.apply(frame.data(), W, H, settings, out);
		std::vector<float> holes(W * H, 0.0f);
		filter.apply(holes.data(), W, H, settings, out);
		check(out[0] == 3000.0f, "dropout held");
		for (int t = 0; t < Filter::kHoldFrames + 1; ++t)
			filter.apply(holes.data(), W, H, settings, out);
		check(out[0] == 0.0f, "hold expires");
	}

	{
		// A pinhole inside a surface is filled; one on an edge is not.
		Filter filter;
		Settings settings;
		settings.fillHoles = true;
		std::vector<float> scene(frame);
		for (int y = 0; y < H; ++y)
			for (int x = 4; x < W; ++x) scene[y * W + x] = 1500.0f;
		scene[2 * W + 1] = 0.0f;  // inside the wall
		scene[5 * W + 4] = 0.0f;  // on the wall / person edge
		filter.apply(scene.data(), W, H, settings, out);
		check(std::fabs(out[2 * W + 1] - 3000.0f) < 1.0f, "hole in a surface filled");
		check(out[5 * W + 4] == 0.0f, "hole on an edge left empty");
	}

	{
		// Resolution change restarts cleanly.
		Filter filter;
		Settings settings;
		settings.smooth = 0.5f;
		filter.apply(frame.data(), W, H, settings, out);
		std::vector<float> small(4 * 4, 1000.0f);
		filter.apply(small.data(), 4, 4, settings, out);
		check(out.size() == 16 && out[0] == 1000.0f, "size change resets state");
	}

	if (failures)
	{
		std::cerr << failures << " depth filter test(s) failed\n";
		return 1;
	}
	std::cout << "Depth filter tests passed\n";
	return 0;
}
