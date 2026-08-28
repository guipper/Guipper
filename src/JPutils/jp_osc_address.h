#pragma once

#include <cctype>
#include <limits>
#include <string>

namespace jp_osc_address
{
	// Parse an address made from an exact prefix plus one non-negative decimal
	// index. No signs, whitespace or trailing path components are accepted.
	// Keeping this independent of openFrameworks makes the OSC contract cheap to
	// exercise in the small tests/ target.
	inline bool indexed(const std::string &address, const std::string &prefix,
		int &index)
	{
		if (address.compare(0, prefix.size(), prefix) != 0 ||
			address.size() == prefix.size())
		{
			return false;
		}
		unsigned long long value = 0;
		for (std::size_t i = prefix.size(); i < address.size(); ++i)
		{
			const unsigned char c =
				static_cast<unsigned char>(address[i]);
			if (!std::isdigit(c)) return false;
			value = value * 10ULL + static_cast<unsigned long long>(c - '0');
			if (value > static_cast<unsigned long long>(
				std::numeric_limits<int>::max())) return false;
		}
		index = static_cast<int>(value);
		return true;
	}
}
