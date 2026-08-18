#include "jp_debug_report.h"

#include <cmath>
#include <cstdlib>

namespace jp_debug
{

unsigned long long fboBytes(int width, int height, int count)
{
	if (width <= 0 || height <= 0 || count <= 0) return 0ull;
	// RGBA8: four bytes a pixel. This is the allocation, not the compressed or
	// mip-mapped footprint, and it deliberately ignores the driver's own
	// padding - it is a floor, useful for spotting "you have 40 of these".
	return (unsigned long long)width * (unsigned long long)height * 4ull *
		(unsigned long long)count;
}

std::string formatBytes(unsigned long long bytes)
{
	const char *units[] = {"B", "KB", "MB", "GB"};
	double value = (double)bytes;
	int unit = 0;
	while (value >= 1024.0 && unit < 3)
	{
		value /= 1024.0;
		++unit;
	}
	// One decimal from KB up; whole bytes below that, where a fraction is noise.
	return unit == 0 ? ofToString((unsigned long long)value) + " B"
					 : ofToString(value, 1) + " " + units[unit];
}

}

namespace jp_debug
{

std::size_t balanceSplit(const std::vector<float> &sectionHeights)
{
	if (sectionHeights.size() < 2) return 1;

	auto sum = [&](std::size_t from, std::size_t to) {
		float total = 0.0f;
		for (std::size_t i = from; i < to && i < sectionHeights.size(); ++i)
			total += sectionHeights[i];
		return total;
	};

	std::size_t best = 1;
	float bestDelta = -1.0f;
	// Every break is tried rather than stopping at the first past halfway:
	// stopping early put 468px against 198px, so the panel stayed as tall as one
	// column and half of it was blank.
	for (std::size_t candidate = 1; candidate < sectionHeights.size(); ++candidate)
	{
		const float delta = std::abs(sum(0, candidate) -
			sum(candidate, sectionHeights.size()));
		if (bestDelta < 0.0f || delta < bestDelta)
		{
			bestDelta = delta;
			best = candidate;
		}
	}
	return best;
}

}
