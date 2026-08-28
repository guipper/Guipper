#include "../src/JPutils/jp_osc_address.h"

#include <iostream>
#include <limits>
#include <string>

int main()
{
	const std::string prefix = "/openguinumber/";
	int index = -1;
	auto accepts = [&](const std::string &address, int expected)
	{
		index = -1;
		return jp_osc_address::indexed(address, prefix, index) &&
			index == expected;
	};
	auto rejects = [&](const std::string &address)
	{
		index = 71;
		return !jp_osc_address::indexed(address, prefix, index) && index == 71;
	};

	if (!accepts("/openguinumber/0", 0) ||
		!accepts("/openguinumber/7", 7) ||
		!accepts("/openguinumber/123", 123) ||
		!rejects("/openguinumber/") ||
		!rejects("/openguinumber/-1") ||
		!rejects("/openguinumber/+1") ||
		!rejects("/openguinumber/ 1") ||
		!rejects("/openguinumber/1/2") ||
		!rejects("/openguinumber/1x") ||
		!rejects("/other/1") ||
		!rejects("/openguinumber/999999999999999999999999"))
	{
		std::cerr << "OSC indexed-address tests failed\n";
		return 1;
	}

	std::cout << "OSC indexed-address tests passed\n";
	return 0;
}
