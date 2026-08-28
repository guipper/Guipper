#include "../src/JPbox/jp_mapping_boolean.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
	int failures = 0;

	void expect(bool condition, const std::string &message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}

	void testCreateAndChain()
	{
		std::vector<JPMappingBooleanGroup> groups;
		const int groupId = jp_mapping_boolean::apply(groups,
			{JPMappingMaskItemKind::Contour, 10}, 20,
			JPMappingBooleanOperation::Difference);
		expect(groupId > 0 && groups.size() == 1,
			"A-B creates one boolean result");
		expect(groups[0].terms.size() == 2 &&
			groups[0].terms[0].contourId == 10 &&
			groups[0].terms[0].operation == JPMappingBooleanOperation::Union &&
			groups[0].terms[1].contourId == 20 &&
			groups[0].terms[1].operation == JPMappingBooleanOperation::Difference,
			"selection order defines A and B");

		const int chained = jp_mapping_boolean::apply(groups,
			{JPMappingMaskItemKind::Group, groupId}, 30,
			JPMappingBooleanOperation::Union);
		expect(chained == groupId && groups[0].terms.size() == 3 &&
			groups[0].terms.back().contourId == 30,
			"a result chains with a standalone contour");
		expect(jp_mapping_boolean::apply(groups,
			{JPMappingMaskItemKind::Contour, 40}, 20,
			JPMappingBooleanOperation::Union) < 0,
			"a grouped contour cannot become a second operand");
	}

	void testRemovalAndSanitize()
	{
		std::vector<JPMappingBooleanGroup> groups = {{7, {
			{1, JPMappingBooleanOperation::Union},
			{2, JPMappingBooleanOperation::Difference},
			{3, JPMappingBooleanOperation::Union}}}};
		jp_mapping_boolean::removeContour(groups, 2);
		expect(groups.size() == 1 && groups[0].terms.size() == 2,
			"removing one term keeps a valid chain");
		jp_mapping_boolean::removeContour(groups, 1);
		expect(groups.empty(), "a one-term result dissolves");

		groups = {
			{2, {{1, JPMappingBooleanOperation::Difference},
				{99, JPMappingBooleanOperation::Union}}},
			{2, {{1, JPMappingBooleanOperation::Union},
				{3, JPMappingBooleanOperation::Difference}}}};
		jp_mapping_boolean::sanitize(groups, {1, 3});
		expect(groups.size() == 1 && groups[0].terms.size() == 2,
			"invalid dissolved groups do not claim contours");
		expect(groups[0].terms.front().operation ==
			JPMappingBooleanOperation::Union,
			"the first chain term is always additive");
	}
}

int main()
{
	testCreateAndChain();
	testRemovalAndSanitize();
	if (failures != 0) return 1;
	std::cout << "mapping boolean tests passed\n";
	return 0;
}
