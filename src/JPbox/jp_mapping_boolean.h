#pragma once

#include <algorithm>
#include <vector>

enum class JPMappingBooleanOperation
{
	Union = 0,
	Difference = 1
};

struct JPMappingBooleanTerm
{
	int contourId = -1;
	JPMappingBooleanOperation operation = JPMappingBooleanOperation::Union;
};

struct JPMappingBooleanGroup
{
	int id = -1;
	std::vector<JPMappingBooleanTerm> terms;
};

enum class JPMappingMaskItemKind
{
	Contour = 0,
	Group = 1
};

struct JPMappingMaskItem
{
	JPMappingMaskItemKind kind = JPMappingMaskItemKind::Contour;
	int id = -1;
};

inline bool operator==(const JPMappingBooleanTerm &a,
	const JPMappingBooleanTerm &b)
{
	return a.contourId == b.contourId && a.operation == b.operation;
}

inline bool operator==(const JPMappingBooleanGroup &a,
	const JPMappingBooleanGroup &b)
{
	return a.id == b.id && a.terms == b.terms;
}

inline bool operator==(const JPMappingMaskItem &a,
	const JPMappingMaskItem &b)
{
	return a.kind == b.kind && a.id == b.id;
}

namespace jp_mapping_boolean
{
	inline int nextGroupId(const std::vector<JPMappingBooleanGroup> &groups)
	{
		int next = 1;
		for (const auto &group : groups) next = std::max(next, group.id + 1);
		return next;
	}

	inline int groupIndexById(const std::vector<JPMappingBooleanGroup> &groups,
		int id)
	{
		for (int i = 0; i < static_cast<int>(groups.size()); ++i)
			if (groups[i].id == id) return i;
		return -1;
	}

	inline int groupIndexForContour(
		const std::vector<JPMappingBooleanGroup> &groups, int contourId)
	{
		for (int i = 0; i < static_cast<int>(groups.size()); ++i)
			for (const auto &term : groups[i].terms)
				if (term.contourId == contourId) return i;
		return -1;
	}

	inline bool containsContour(const std::vector<int> &ids, int id)
	{
		return std::find(ids.begin(), ids.end(), id) != ids.end();
	}

	// A result may be chained with one standalone contour. Keeping the right
	// operand simple makes the expression an ordered raster chain rather than a
	// tree, so it remains cheap to evaluate at output resolution.
	inline int apply(std::vector<JPMappingBooleanGroup> &groups,
		const JPMappingMaskItem &left, int rightContourId,
		JPMappingBooleanOperation operation)
	{
		if (rightContourId < 0 ||
			groupIndexForContour(groups, rightContourId) >= 0)
			return -1;

		if (left.kind == JPMappingMaskItemKind::Group)
		{
			const int groupIndex = groupIndexById(groups, left.id);
			if (groupIndex < 0) return -1;
			for (const auto &term : groups[groupIndex].terms)
				if (term.contourId == rightContourId) return -1;
			groups[groupIndex].terms.push_back({rightContourId, operation});
			return groups[groupIndex].id;
		}

		if (left.id < 0 || left.id == rightContourId ||
			groupIndexForContour(groups, left.id) >= 0)
			return -1;
		JPMappingBooleanGroup group;
		group.id = nextGroupId(groups);
		group.terms.push_back({left.id, JPMappingBooleanOperation::Union});
		group.terms.push_back({rightContourId, operation});
		groups.push_back(group);
		return group.id;
	}

	inline void removeContour(std::vector<JPMappingBooleanGroup> &groups,
		int contourId)
	{
		for (auto &group : groups)
		{
			group.terms.erase(std::remove_if(group.terms.begin(), group.terms.end(),
				[&](const JPMappingBooleanTerm &term) {
					return term.contourId == contourId;
				}), group.terms.end());
			if (!group.terms.empty())
				group.terms.front().operation = JPMappingBooleanOperation::Union;
		}
		groups.erase(std::remove_if(groups.begin(), groups.end(),
			[](const JPMappingBooleanGroup &group) {
				return group.terms.size() < 2;
			}), groups.end());
	}

	inline void sanitize(std::vector<JPMappingBooleanGroup> &groups,
		const std::vector<int> &validContourIds)
	{
		std::vector<int> claimed;
		std::vector<int> groupIds;
		std::vector<JPMappingBooleanGroup> validGroups;
		int generatedId = nextGroupId(groups);
		for (auto group : groups)
		{
			if (group.id < 0 || containsContour(groupIds, group.id))
				group.id = generatedId++;
			std::vector<int> local;
			group.terms.erase(std::remove_if(group.terms.begin(), group.terms.end(),
				[&](const JPMappingBooleanTerm &term) {
					if (!containsContour(validContourIds, term.contourId) ||
						containsContour(claimed, term.contourId) ||
						containsContour(local, term.contourId)) return true;
					local.push_back(term.contourId);
					return false;
				}), group.terms.end());
			if (group.terms.size() < 2) continue;
			group.terms.front().operation = JPMappingBooleanOperation::Union;
			claimed.insert(claimed.end(), local.begin(), local.end());
			groupIds.push_back(group.id);
			validGroups.push_back(group);
		}
		groups.swap(validGroups);
	}
}
