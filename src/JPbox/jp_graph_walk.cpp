#include "jp_graph_walk.h"
#include "jp_box.h"
#include <algorithm>

void jp_graphwalk::findConsumers(const std::vector<JPbox *> &list,
	const JPbox *producer, std::vector<JPbox *> &out)
{
	out.clear();
	if (producer == nullptr) return;
	// const_cast only to take the address: fbo is a non-const member and the
	// inlets store a non-const ofFbo*, but nothing here writes through it.
	const ofFbo *output = &const_cast<JPbox *>(producer)->fbo;
	for (JPbox *consumer : list)
	{
		if (consumer == nullptr || consumer == producer) continue;
		for (int inlet = 0; inlet < consumer->fbohandlergroup.getSize(); ++inlet)
		{
			if (!consumer->fbohandlergroup.getisPointerSet(inlet)) continue;
			if (consumer->fbohandlergroup.getFboPointerReference(inlet) != output)
				continue;
			out.push_back(consumer);
			break; // one entry per consumer, however many inlets it wires up
		}
	}
}

JPbox *jp_graphwalk::firstConsumer(const std::vector<JPbox *> &list,
	const JPbox *producer)
{
	std::vector<JPbox *> consumers;
	findConsumers(list, producer, consumers);
	return consumers.empty() ? nullptr : consumers.front();
}

JPbox *jp_graphwalk::resolveChainTerminal(const std::vector<JPbox *> &list,
	JPbox *start, std::vector<JPbox *> *outPath)
{
	if (outPath != nullptr) outPath->clear();
	if (start == nullptr) return nullptr;
	std::vector<JPbox *> seen;
	seen.push_back(start);
	if (outPath != nullptr) outPath->push_back(start);
	JPbox *current = start;
	for (int hop = 0; hop < kMaxHops; ++hop)
	{
		JPbox *next = firstConsumer(list, current);
		if (next == nullptr) break;
		// A cycle: stop on the box we came from rather than looping forever.
		if (std::find(seen.begin(), seen.end(), next) != seen.end()) break;
		seen.push_back(next);
		if (outPath != nullptr) outPath->push_back(next);
		current = next;
	}
	return current;
}
