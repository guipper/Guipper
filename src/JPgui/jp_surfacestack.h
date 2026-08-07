#pragma once

#include "ofMain.h"
#include <functional>
#include <vector>

// Every floating thing in the program - panels, dropdowns, the save modal, an
// editing text field - is a "surface". Before this existed each one was an
// independent bool and the question "who is on top / who blocks input / what
// does ESC close" was answered by the hand-written order of early returns in
// ofApp::mousePressed and ofApp::keyPressed. That is why the save modal was not
// actually modal, why ESC meant eight different things, and why the cue panel
// was drawn under the boxes but still ate their clicks.
//
// A surface declares its own z-order and modality once, here, and the rules
// derive from that. This is deliberately an adapter over the flags that already
// exist rather than a rewrite: isOpen/close just read and clear them.
struct JPSurface
{
	int id = -1;
	// Higher is nearer the viewer. Also decides what ESC closes first.
	int order = 0;
	// Blocks the mouse for everything underneath, not just for its own rect.
	bool modal = false;
	std::function<bool()> isOpen;
	std::function<void()> close;
	// Screen rect the surface occupies. An empty rect means "no region" - the
	// surface still stacks and still answers to ESC, but it never blocks a
	// click by position (an editing text field, for instance).
	std::function<ofRectangle()> bounds;
};

class JPSurfaceStack
{
public:
	void add(const JPSurface &surface)
	{
		surfaces.push_back(surface);
	}

	bool isOpen(int id) const
	{
		const JPSurface *s = find(id);
		return s != nullptr && s->isOpen && s->isOpen();
	}

	bool anyOpen() const
	{
		return topmost() >= 0;
	}

	// Id of the open surface with the highest order, or -1 when none is open.
	int topmost() const
	{
		int bestId = -1;
		int bestOrder = 0;
		for (const JPSurface &s : surfaces)
		{
			if (!s.isOpen || !s.isOpen()) continue;
			if (bestId < 0 || s.order > bestOrder)
			{
				bestId = s.id;
				bestOrder = s.order;
			}
		}
		return bestId;
	}

	// The single ESC rule: dismiss one layer. Returns false when nothing was
	// open, so the caller can leave ESC alone rather than inventing a meaning
	// for it.
	bool closeTopmost()
	{
		const int id = topmost();
		if (id < 0) return false;
		const JPSurface *s = find(id);
		if (s == nullptr || !s->close) return false;
		s->close();
		return true;
	}

	bool modalOpen() const
	{
		for (const JPSurface &s : surfaces)
		{
			if (s.modal && s.isOpen && s.isOpen()) return true;
		}
		return false;
	}

	int modalOrder() const
	{
		int order = 0;
		bool found = false;
		for (const JPSurface &s : surfaces)
		{
			if (!s.modal || !s.isOpen || !s.isOpen()) continue;
			if (!found || s.order > order) { order = s.order; found = true; }
		}
		return found ? order : -1;
	}

	// True when a click at (x,y) belongs to something stacked above `order`, so
	// a layer at `order` must not act on it. Generalises the old
	// JPboxgroup::mouseOverGui(), which knew about exactly two rects.
	bool blockedAt(float x, float y, int order) const
	{
		const int modal = modalOrder();
		// A modal blocks the whole window, not only its own rect.
		if (modal >= 0 && order < modal) return true;
		for (const JPSurface &s : surfaces)
		{
			if (s.order <= order) continue;
			if (!s.isOpen || !s.isOpen()) continue;
			if (!s.bounds) continue;
			const ofRectangle r = s.bounds();
			if (r.getWidth() <= 0.0f || r.getHeight() <= 0.0f) continue;
			if (r.inside(x, y)) return true;
		}
		return false;
	}

	// Close everything at or above `order`. Used when entering a context that
	// the open surfaces do not belong to.
	void closeFrom(int order)
	{
		for (const JPSurface &s : surfaces)
		{
			if (s.order < order) continue;
			if (s.isOpen && s.isOpen() && s.close) s.close();
		}
	}

private:
	const JPSurface *find(int id) const
	{
		for (const JPSurface &s : surfaces)
		{
			if (s.id == id) return &s;
		}
		return nullptr;
	}

	std::vector<JPSurface> surfaces;
};
