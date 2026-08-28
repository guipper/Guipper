#pragma once

#include <vector>

class JPbox;

// Walking the patch DOWNSTREAM.
//
// An edge is stored once, on the CONSUMER: consumer.fbohandlergroup[inlet].fbo
// points at &producer->fbo. There is no outlet list, so "who reads me" is a
// scan of every box's inlets - the loop that was already written by hand in
// jp_renderschedule::apply, producerUidForInlet and detachBoxFromView. This is
// that loop, once, in the consumer direction.
//
// MATCHED BY FBO POINTER, NEVER BY NAME. Two boxes may carry the same display
// name (renaming is a bare assignment), and a name match picks whichever one
// the loop reached first. collectCueDraftPath and draw_conections still match
// by name and are wrong in exactly that case; do not copy them.
//
// Scope is ONE list - the main graph, or one preset's children. A child's FBO
// can only appear in a sibling's inlet, so a chain inside a group ends inside
// that group. It leaves only through the group's activeRender, and following it
// out would silently substitute the group composite for the chain the user drew.
namespace jp_graphwalk
{
	// Belt and braces on top of the visited set. Nothing in this program
	// rejects a multi-hop cycle when connecting - jp_fbohandler only refuses a
	// box feeding itself - so A->B->C->A is fully constructible.
	constexpr int kMaxHops = 64;

	void findConsumers(const std::vector<JPbox *> &list, const JPbox *producer,
		std::vector<JPbox *> &out);

	// Lowest index wins, which is stable frame to frame.
	JPbox *firstConsumer(const std::vector<JPbox *> &list,
		const JPbox *producer);

	// Last box of the chain that starts at `start`, taking the first consumer
	// at every hop. Returns `start` itself when nothing reads it. `outPath`,
	// when given, receives start..terminal inclusive.
	JPbox *resolveChainTerminal(const std::vector<JPbox *> &list, JPbox *start,
		std::vector<JPbox *> *outPath = nullptr);
}
