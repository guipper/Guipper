#include "../src/JPutils/jp_graph_history.h"

#include <iostream>
#include <map>
#include <string>

namespace
{
	int failures = 0;

	void expect(bool condition, const std::string &message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}

	// Stands in for the live graph. Boxes are ints on the heap so the test can
	// prove eviction really destroys what the ring was holding: leaking one shows
	// up as a live count that never returns to zero.
	class FakeGraph : public JPGraphHistoryTarget
	{
	public:
		std::map<std::string, float> positions;
		int applied = 0;
		int reverted = 0;
		int released = 0;
		int liveBoxes = 0;
		bool refuse = false;

		void *makeBox()
		{
			++liveBoxes;
			return new int(1);
		}

		bool applyGraphEdit(JPGraphEdit &edit) override
		{
			if (refuse) return false;
			++applied;
			for (const JPGraphEdit::BoxMove &move : edit.moves)
				positions[move.uid] = move.toX;
			return true;
		}

		bool revertGraphEdit(JPGraphEdit &edit) override
		{
			if (refuse) return false;
			++reverted;
			for (const JPGraphEdit::BoxMove &move : edit.moves)
				positions[move.uid] = move.fromX;
			return true;
		}

		void releaseGraphEdit(JPGraphEdit &edit, bool) override
		{
			++released;
			for (JPGraphDetachedBox &entry : edit.detached)
			{
				if (entry.box == nullptr) continue;
				delete static_cast<int *>(entry.box);
				entry.box = nullptr;
				--liveBoxes;
			}
			for (JPGraphEdit::GroupPayload &payload : edit.groups)
			{
				if (payload.groupBox == nullptr) continue;
				delete static_cast<int *>(payload.groupBox);
				payload.groupBox = nullptr;
				--liveBoxes;
			}
		}
	};

	JPGraphEdit moveEdit(const std::string &uid, float from, float to)
	{
		JPGraphEdit edit;
		edit.kind = JPGraphEdit::MoveBoxes;
		JPGraphEdit::BoxMove move;
		move.uid = uid;
		move.fromX = from;
		move.toX = to;
		edit.moves.push_back(move);
		return edit;
	}

	JPGraphEdit deleteEdit(FakeGraph &graph, std::size_t boxCount)
	{
		JPGraphEdit edit;
		edit.kind = JPGraphEdit::DeleteBoxes;
		for (std::size_t i = 0; i < boxCount; ++i)
		{
			JPGraphDetachedBox detached;
			detached.box = graph.makeBox();
			detached.uid = "d" + std::to_string(i);
			detached.index = (int)i;
			edit.detached.push_back(detached);
		}
		return edit;
	}

	JPGraphEdit groupEdit(FakeGraph &graph, std::size_t groupCount, bool inverted)
	{
		JPGraphEdit edit;
		edit.kind = JPGraphEdit::GroupBoxes;
		edit.inverted = inverted;
		for (std::size_t i = 0; i < groupCount; ++i)
		{
			JPGraphEdit::GroupPayload payload;
			payload.groupBox = graph.makeBox();
			payload.groupUid = "g" + std::to_string(i);
			payload.groupIndex = (int)i;
			edit.groups.push_back(payload);
		}
		return edit;
	}

	// Ungrouping a multi-selection is ONE entry carrying several groups, so the
	// budget has to count them all - otherwise the ring thinks a five-group
	// ungroup costs the same as a one-group one and holds five times the memory
	// it budgeted for.
	void testGroupPayloadAccounting()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(groupEdit(graph, 3, true));
		expect(ring.retainedBoxes() == 3,
			"a three-group ungroup counts as three retained boxes");
		expect(graph.liveBoxes == 3, "and all three are alive");

		ring.clear();
		expect(graph.liveBoxes == 0, "clear destroyed every group box");
	}

	// Ownership is mirrored between the two directions. Getting this backwards
	// would either leak a group box or free one the graph is still using.
	void testGroupOwnershipPolarity()
	{
		{
			// A grouping that STANDS: the graph owns the group, the ring must
			// not free it.
			FakeGraph graph;
			JPGraphUndoRing ring(&graph);
			ring.push(groupEdit(graph, 1, false));
			ring.clear();
			expect(graph.liveBoxes == 0,
				"clearing a standing grouping still released the box it held");
		}
		{
			// An ungrouping that stands: the ring owns the dissolved group.
			FakeGraph graph;
			JPGraphUndoRing ring(&graph);
			ring.push(groupEdit(graph, 1, true));
			expect(ring.retainedBoxes() == 1, "the dissolved group is retained");
			ring.undo();
			expect(ring.canRedo(), "the ungroup can be redone");
			ring.clear();
			expect(graph.liveBoxes == 0, "clear released it either way");
		}
	}

	JPGraphEdit paramEdit(const std::string &uid, int index,
		float before, float after, unsigned long long stamp)
	{
		JPGraphEdit edit;
		edit.kind = JPGraphEdit::SetParameter;
		edit.boxUid = uid;
		edit.paramIndex = index;
		edit.paramBefore.floatValue = before;
		edit.paramAfter.floatValue = after;
		edit.stampMs = stamp;
		return edit;
	}

	// amendableNewest is what lets a stream of changes collapse into one step.
	// A MIDI knob reports every step of its travel, so without it a single sweep
	// would push dozens of entries and evict the rest of the history.
	void testAmendableNewest()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		expect(ring.amendableNewest() == nullptr,
			"an empty ring has nothing to amend");

		ring.push(paramEdit("a", 0, 0.0f, 0.1f, 1000));
		JPGraphEdit *newest = ring.amendableNewest();
		expect(newest != nullptr, "the entry just pushed is amendable");

		// Fold a sweep into the one entry, keeping the ORIGINAL before.
		if (newest != nullptr)
		{
			newest->paramAfter.floatValue = 0.9f;
			newest->stampMs = 1200;
		}
		expect(ring.size() == 1, "amending did not add an entry");
		newest = ring.amendableNewest();
		expect(newest != nullptr && newest->paramBefore.floatValue == 0.0f,
			"undo would land where the gesture started, not one message back");
		expect(newest != nullptr && newest->paramAfter.floatValue == 0.9f,
			"the amended entry carries the newest value");

		// Once something has been undone there IS a redo tail, and rewriting the
		// entry under the cursor would silently change a step the user can still
		// walk forward into.
		expect(ring.undo(), "undo");
		expect(ring.amendableNewest() == nullptr,
			"nothing is amendable while a redo tail exists");

		expect(ring.redo(), "redo");
		expect(ring.amendableNewest() != nullptr,
			"amending is available again once the tail is consumed");
	}

	void testUndoRedoCycle()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		expect(!ring.canUndo(), "a fresh ring has nothing to undo");
		expect(!ring.canRedo(), "a fresh ring has nothing to redo");

		graph.positions["a"] = 10.0f;
		ring.push(moveEdit("a", 10.0f, 40.0f));
		graph.positions["a"] = 40.0f;

		expect(ring.canUndo(), "a pushed edit is undoable");
		expect(!ring.canRedo(), "a pushed edit is not redoable yet");

		expect(ring.undo(), "undo ran");
		expect(graph.positions["a"] == 10.0f, "undo restored the old position");
		expect(!ring.canUndo(), "the ring is back at the start");
		expect(ring.canRedo(), "the undone edit became redoable");

		expect(ring.redo(), "redo ran");
		expect(graph.positions["a"] == 40.0f, "redo reapplied the new position");
		expect(!ring.canRedo(), "nothing left to redo");
	}

	// Undo and redo must be exact inverses over a long alternating run, not just
	// for a single step - a cursor that drifts by one only shows up here.
	void testInverseOverManySteps()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		for (int i = 0; i < 20; ++i)
			ring.push(moveEdit("a", (float)i, (float)(i + 1)));

		for (int i = 0; i < 20; ++i) expect(ring.undo(), "undo step");
		expect(!ring.canUndo(), "every step was undone");
		expect(graph.positions["a"] == 0.0f, "the graph is back at its origin");

		for (int i = 0; i < 20; ++i) expect(ring.redo(), "redo step");
		expect(!ring.canRedo(), "every step was redone");
		expect(graph.positions["a"] == 20.0f, "the graph is back at the newest state");
	}

	void testPushTruncatesRedoTail()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(moveEdit("a", 0.0f, 1.0f));
		ring.push(moveEdit("a", 1.0f, 2.0f));
		expect(ring.undo(), "undo before branching");
		expect(ring.canRedo(), "there is a redo tail to lose");

		ring.push(moveEdit("a", 1.0f, 9.0f));
		expect(!ring.canRedo(), "pushing dropped the unreachable redo tail");
		expect(ring.size() == 2, "the branched entry replaced the tail");
	}

	// Adding a box is deliberately not undoable, but it still has to invalidate
	// the redo tail: redoing across an untracked mutation would apply an edit to
	// a graph that no longer matches what the edit was recorded against.
	void testInvalidateRedo()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(moveEdit("a", 0.0f, 1.0f));
		expect(ring.undo(), "undo");
		expect(ring.canRedo(), "redo is available");

		ring.invalidateRedo();
		expect(!ring.canRedo(), "an untracked mutation dropped the redo tail");
		expect(ring.size() == 0, "the entry itself is gone");
		expect(!ring.canUndo(), "and there is nothing left to undo");
	}

	void testEntryEviction()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		for (std::size_t i = 0; i < JPGraphUndoRing::kMaxEntries + 25; ++i)
			ring.push(moveEdit("a", (float)i, (float)(i + 1)));

		expect(ring.size() == JPGraphUndoRing::kMaxEntries,
			"the ring stopped growing at its entry cap");
		expect(ring.cursorPosition() == JPGraphUndoRing::kMaxEntries,
			"eviction moved the cursor down with the entries");
		expect(graph.released == 25, "the 25 oldest steps were released");

		// The surviving history must still be walkable end to end.
		std::size_t steps = 0;
		while (ring.undo()) ++steps;
		expect(steps == JPGraphUndoRing::kMaxEntries,
			"every surviving entry undid cleanly");
	}

	// The entry cap alone cannot bound memory: each detached box keeps a
	// full-resolution FBO alive. This is the second bound doing its job.
	void testRetainedBoxEviction()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		for (std::size_t i = 0; i < JPGraphUndoRing::kMaxRetainedBoxes + 5; ++i)
			ring.push(deleteEdit(graph, 1));

		expect(ring.retainedBoxes() <= JPGraphUndoRing::kMaxRetainedBoxes,
			"retained boxes stayed within budget");
		expect(ring.size() == JPGraphUndoRing::kMaxRetainedBoxes,
			"entries were dropped to honour the box budget, not the entry cap");
		expect(graph.liveBoxes == (int)JPGraphUndoRing::kMaxRetainedBoxes,
			"evicted boxes were really destroyed, not leaked");

		ring.clear();
		expect(graph.liveBoxes == 0, "clear destroyed every retained box");
		expect(ring.retainedBoxes() == 0, "clear reset the budget");
	}

	// One delete of several boxes is one entry, so a multi-selection comes back
	// in a single Ctrl+Z rather than one press per box.
	void testMultiBoxDeleteIsOneEntry()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(deleteEdit(graph, 4));
		expect(ring.size() == 1, "four boxes deleted together are one step");
		expect(ring.retainedBoxes() == 4, "all four are retained");

		expect(ring.undo(), "one undo restores the whole selection");
		expect(!ring.canUndo(), "and there is no second step hiding behind it");
	}

	// A target that refuses must leave the cursor alone, or the ring and the graph
	// disagree about what is currently applied.
	void testRefusedEditLeavesCursor()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(moveEdit("a", 0.0f, 1.0f));
		graph.refuse = true;
		expect(!ring.undo(), "a refused revert reports failure");
		expect(ring.canUndo(), "the entry is still pending");
		expect(ring.cursorPosition() == 1, "the cursor did not move");

		graph.refuse = false;
		expect(ring.undo(), "the same entry undoes once the target accepts");
	}

	void testClearedRingIsInert()
	{
		FakeGraph graph;
		JPGraphUndoRing ring(&graph);

		ring.push(moveEdit("a", 0.0f, 1.0f));
		ring.clear();
		expect(!ring.canUndo(), "a cleared ring has nothing to undo");
		expect(!ring.canRedo(), "a cleared ring has nothing to redo");
		expect(!ring.undo(), "undo on a cleared ring is a no-op");
		expect(ring.size() == 0, "a cleared ring is empty");
	}
}

int main()
{
	testUndoRedoCycle();
	testAmendableNewest();
	testGroupPayloadAccounting();
	testGroupOwnershipPolarity();
	testInverseOverManySteps();
	testPushTruncatesRedoTail();
	testInvalidateRedo();
	testEntryEviction();
	testRetainedBoxEviction();
	testMultiBoxDeleteIsOneEntry();
	testRefusedEditLeavesCursor();
	testClearedRingIsInert();
	if (failures != 0)
	{
		std::cerr << failures << " graph history test(s) failed\n";
		return 1;
	}
	std::cout << "graph history tests passed\n";
	return 0;
}
