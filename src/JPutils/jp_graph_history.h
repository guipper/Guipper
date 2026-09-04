#pragma once

#include "../JPbox/jp_media_state.h"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Bounded undo/redo for the node graph.
//
// A command ring, not document snapshots. A whole-composition snapshot is not
// usable as an undo step here: JPboxgroup::load() begins by destroying every box
// and rebuilds each one through setup(), which recompiles every .frag, reallocates
// a full-resolution FBO per box, reopens each video's GStreamer pipeline and
// re-enumerates the cameras. That is hundreds of milliseconds at best, with black
// frames and reset video playheads, and it silently drops state that is never
// serialized at all (controllers, cue, morph, the active group path).
//
// So each entry records ONE reversible mutation and carries whatever payload its
// inverse needs, exactly like JPPaintEdit does for the paint document.
//
// Deliberately free of any openFrameworks dependency, for the same reason
// jp_paint_doc.h is: tests/ compiles this header on its own. Live boxes therefore
// appear here only as opaque void* handles - the ring budgets and owns them, but
// only JPboxgroup ever dereferences one.

// The slice of a JPParameter an edit can change. Mirroring the whole class would
// drag ofMain.h in; these are the fields the inspector and the sliders write.
struct JPGraphParamState
{
	float floatValue = 0.0f;
	float lerpValue = 0.0f;
	bool boolValue = false;
	int movtype = 0;
	int lastMovtype = 0;
	int bpmRate = 0;
	float speed = 0.0f;
	float min = 0.0f;
	float max = 0.0f;
	bool rangeEnabled = false;
	bool randomLocked = false;
	float defaultFloat = 0.0f;
	bool defaultBool = false;
};

// One FINAL-stack layer that left the stack with its box.
//
// Flattened rather than holding a JPQuickImageLayerState, because that struct
// carries ofVec2f and would drag ofMain.h into this header - which exists to be
// compilable on its own, and has a test binary that proves it still is.
// jp_media_state.h is engine-free for the same reason, so the transport state
// travels whole rather than being flattened too.
struct JPGraphDetachedFinalLayer
{
	// Where it sat in the stack. The stack IS the draw order, so putting the
	// layer back at the end on undo would silently move it to the top.
	int index = 0;
	unsigned long long id = 0;
	std::string sourceUid;
	std::string path;
	std::string name;
	bool visible = true;
	bool followChain = true;
	float opacity = 1.0f;
	float centerX = 0.5f;
	float centerY = 0.5f;
	float sizeX = 1.0f;
	float sizeY = 1.0f;
	float rotationDegrees = 0.0f;
	JPMediaState media;
};

// One box that has been taken out of a graph but NOT destroyed.
//
// Not destroying is the whole point: a destroyed shader box costs a recompile to
// bring back and a destroyed video box restarts its playhead, so "undo delete"
// would be neither instant nor lossless. Keeping the object alive also keeps its
// address stable, which is what lets severed inlets be restored by handing back
// the very same &box->fbo pointer.
struct JPGraphDetachedBox
{
	void *box = nullptr; // JPbox *
	std::string uid;
	// Where it sat in its view's box list, so undo puts it back in place rather
	// than at the end - box order drives activeRender and the exposed-parameter
	// arrays that run parallel to it.
	int index = 0;
	// Inlets elsewhere in the view that pointed AT this box and were cut when it
	// left: (consumer uid, inlet index). Without these, undoing a delete brings
	// the box back with every cable into it missing.
	std::vector<std::pair<std::string, int>> severedInlets;
	// A group's public texture inputs that pointed at this box and were dropped
	// with it: (publicName, targetBoxName, targetSamplerName). Undo has to hand
	// these back too, or the group silently loses an inlet its parent was wiring
	// into.
	struct ExposedInput
	{
		std::string publicName;
		std::string targetBoxName;
		std::string targetSamplerName;
	};
	std::vector<ExposedInput> exposedInputs;
	// FINAL-stack layers this box (or, for a group, any box inside it) was
	// feeding. They leave with it and come back with it, exactly like the
	// severed inlets above: a layer whose source no longer exists draws nothing
	// and is unremovable from the panel except by hand.
	//
	// Held in ASCENDING index order, so restoring walks it forwards.
	std::vector<JPGraphDetachedFinalLayer> finalLayers;
};

struct JPGraphEdit
{
	// In-memory only, never written to a savefile, so unlike the persisted enums
	// in this codebase these may be reordered freely.
	enum Kind
	{
		MoveBoxes = 0,
		SetParameter,
		SetConnection,
		ReorderInput,
		SetBoxState,
		SetActiveRender,
		DeleteBoxes,
		GroupBoxes,
	};

	int kind = MoveBoxes;

	// Which view resolves this entry's uids. Empty is the main graph, otherwise
	// the uid of the group whose children the entry addresses.
	//
	// The VIEW travels with the entry rather than with the ring, which is what
	// lets one ring cover the whole composition. A per-view ring meant undoing a
	// grouping destroyed that group's own stack, and it made Ctrl+Z mean "undo
	// the last thing I did HERE" rather than "undo the last thing I did".
	//
	// Safe because the timeline is linear: to reach an entry recorded inside a
	// group you must first undo everything after it, including whatever deleted
	// or dissolved that group - which puts it back. Eviction cannot break it
	// either, since it drops the OLDEST entries first and an entry inside a
	// group is always older than the one that disposed of it.
	std::string viewUid;
	// The cue draft is a view of its own whose boxes are clones, living in
	// cueState rather than in any preset. It keeps a separate ring for the same
	// reason: those clones are destroyed when the cue is applied or dropped.
	bool cueDraft = false;
	// When this entry was recorded, in milliseconds since app start. Only used to
	// decide whether an incoming change continues the same gesture - see
	// amendableNewest(). Plain integer so this header keeps no dependency on any
	// clock of its own.
	unsigned long long stampMs = 0;

	// --- MoveBoxes -------------------------------------------------------
	struct BoxMove
	{
		std::string uid;
		float fromX = 0.0f;
		float fromY = 0.0f;
		float toX = 0.0f;
		float toY = 0.0f;
	};
	// A multi-box drag is ONE entry. Undoing half a gesture would be surprising,
	// and it is the same reason a paint dab and its symmetry mirror share one.
	std::vector<BoxMove> moves;

	// --- SetParameter / SetConnection / ReorderInput / SetBoxState -------
	std::string boxUid;

	int paramIndex = -1;
	JPGraphParamState paramBefore;
	JPGraphParamState paramAfter;

	// SetConnection: boxUid is the CONSUMER, the uids below are the producer
	// before and after. An empty producer means "no cable".
	int inletIndex = -1;
	std::string producerBefore;
	std::string producerAfter;

	// ReorderInput: the two inlets that swapped (inletIndex and this one).
	int inletSecond = -1;

	bool onoffBefore = false;
	bool onoffAfter = false;
	bool bypassBefore = false;
	bool bypassAfter = false;

	// --- SetActiveRender -------------------------------------------------
	// Which box the view renders. Stored as a uid with the index as a fallback:
	// the uid survives the list being reordered, but "no box is active" is a
	// real state and has no uid to name it.
	std::string activeUidBefore;
	std::string activeUidAfter;
	int activeIndexBefore = -1;
	int activeIndexAfter = -1;

	// --- DeleteBoxes -----------------------------------------------------
	// Owned by the ring while the delete stands; owned by the graph again once
	// undone. Eviction is the only thing that ever destroys them.
	std::vector<JPGraphDetachedBox> detached;

	// --- GroupBoxes ------------------------------------------------------
	// Grouping is the one box creation that IS undoable - it is a structural
	// move, not an "add box".
	struct GroupPayload
	{
		void *groupBox = nullptr; // JPbox_preset *
		std::string groupUid;
		int groupIndex = -1;
		// Each child's uid and the index it held in the parent view outside the
		// group, so the move can be replayed in either direction.
		std::vector<JPGraphDetachedBox> members;
		// The group box' OWN FINAL layers. Dissolving takes the group box out
		// of the graph while its children stay, so only the group's uid dies -
		// and a layer naming it would be left pointing at nothing.
		std::vector<JPGraphDetachedFinalLayer> finalLayers;
	};
	// Held in DESCENDING groupIndex order, which is the order they must be
	// dissolved in - taking one out shifts everything after it. Forming them
	// walks the list backwards.
	//
	// A list rather than a single group because ungrouping a multi-selection has
	// to come back with one Ctrl+Z, not one press per group.
	std::vector<GroupPayload> groups;
	// An ungroup is this same edit played backwards, so it reuses the kind with
	// the two directions swapped rather than duplicating the pointer surgery -
	// the part where a mistake costs a crash. It also flips who owns the group
	// boxes: for a grouping the history holds them while the edit is UNDONE, for
	// an ungrouping while the edit STANDS.
	bool inverted = false;
};

// How many live boxes this entry can ever own. Constant regardless of which side
// of the cursor the entry currently sits on, so the ring's budget does not shift
// under it every time the user undoes a step.
inline std::size_t jp_graphRetainedCapacity(const JPGraphEdit &edit)
{
	if (edit.kind == JPGraphEdit::DeleteBoxes) return edit.detached.size();
	if (edit.kind == JPGraphEdit::GroupBoxes) return edit.groups.size();
	return 0u;
}

// What the ring needs from the graph. JPboxgroup implements it; the tests use a
// fake, which is the point of keeping the dependency this thin.
//
// The ring knows nothing about views: each entry names its own (see viewUid),
// so one ring covers the whole composition.
class JPGraphHistoryTarget
{
public:
	virtual ~JPGraphHistoryTarget() = default;
	// Redo direction: put the edit back into effect.
	virtual bool applyGraphEdit(JPGraphEdit &edit) = 0;
	// Undo direction.
	virtual bool revertGraphEdit(JPGraphEdit &edit) = 0;
	// The entry is leaving the ring for good. Destroy whatever it still owns.
	// `wasApplied` says which side of the cursor it was on, which is what decides
	// ownership: a standing delete owns its boxes, an undone group owns the group.
	virtual void releaseGraphEdit(JPGraphEdit &edit, bool wasApplied) = 0;
};

class JPGraphUndoRing
{
public:
	static constexpr std::size_t kMaxEntries = 100;
	// Bounded a second time, on retained boxes. The entry count alone is not
	// enough: every detached box keeps its full-resolution FBO alive, which is
	// megabytes each, so a hundred deletes would hold far more memory than the
	// composition itself.
	static constexpr std::size_t kMaxRetainedBoxes = 12;

	explicit JPGraphUndoRing(JPGraphHistoryTarget *_target = nullptr)
		: target(_target) {}

	~JPGraphUndoRing() { clear(); }

	JPGraphUndoRing(const JPGraphUndoRing &) = delete;
	JPGraphUndoRing &operator=(const JPGraphUndoRing &) = delete;

	// A ring with no target silently accepts pushes and refuses undo.
	void bind(JPGraphHistoryTarget *_target) { target = _target; }

	// Record an edit the caller has ALREADY applied to the graph.
	void push(JPGraphEdit &&edit)
	{
		dropRedoTail();
		retained += jp_graphRetainedCapacity(edit);
		entries.push_back(std::move(edit));
		++cursor;
		evict();
	}

	// Untracked mutations - adding a box is the one the user asked to keep out of
	// the history - must still invalidate the redo tail. Leaving it in place would
	// let a later redo run against a graph that changed behind its back.
	void invalidateRedo() { dropRedoTail(); }

	// The newest entry, but only when it is safe to rewrite in place: something
	// must have been recorded, and nothing may have been undone since. With a
	// redo tail present, amending would silently change a step the user can still
	// walk forward into.
	//
	// This is what lets a continuous stream of changes - a MIDI knob sends one
	// message per step of its travel - collapse into the single gesture the user
	// actually performed, instead of flooding the ring and evicting everything
	// else within a couple of seconds.
	JPGraphEdit *amendableNewest()
	{
		if (cursor == 0 || cursor != entries.size()) return nullptr;
		return &entries[cursor - 1];
	}

	bool canUndo() const { return cursor > 0; }
	bool canRedo() const { return cursor < entries.size(); }

	bool undo()
	{
		if (!canUndo() || target == nullptr) return false;
		if (!target->revertGraphEdit(entries[cursor - 1])) return false;
		--cursor;
		return true;
	}

	bool redo()
	{
		if (!canRedo() || target == nullptr) return false;
		if (!target->applyGraphEdit(entries[cursor])) return false;
		++cursor;
		return true;
	}

	void clear()
	{
		for (std::size_t i = 0; i < entries.size(); ++i) release(i);
		entries.clear();
		cursor = 0;
		retained = 0;
	}

	std::size_t size() const { return entries.size(); }
	std::size_t retainedBoxes() const { return retained; }
	std::size_t cursorPosition() const { return cursor; }

private:
	void release(std::size_t index)
	{
		if (target == nullptr) return;
		target->releaseGraphEdit(entries[index], index < cursor);
	}

	void dropRedoTail()
	{
		while (entries.size() > cursor)
		{
			release(entries.size() - 1);
			retained -= jp_graphRetainedCapacity(entries.back());
			entries.pop_back();
		}
	}

	// Drops from the front, so what the user loses is their oldest step rather
	// than their newest.
	void evict()
	{
		while (!entries.empty() &&
			(entries.size() > kMaxEntries || retained > kMaxRetainedBoxes))
		{
			release(0);
			retained -= jp_graphRetainedCapacity(entries.front());
			entries.erase(entries.begin());
			if (cursor > 0) --cursor;
		}
	}

	JPGraphHistoryTarget *target = nullptr;
	std::vector<JPGraphEdit> entries;
	// entries[0, cursor) are in effect; entries[cursor, end) are redoable.
	std::size_t cursor = 0;
	std::size_t retained = 0;
};
