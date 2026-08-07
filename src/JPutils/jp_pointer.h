#pragma once

#include "ofMain.h"
#include <functional>

// Which layer owns the pointer right now.
//
// Nothing in the app knew this. Every hit test was a bare rectangle check on
// the mouse position, so a node box lit up and responded through the inspector,
// and a button under an open dropdown could be pressed by clicking the dropdown.
// Worse, JPToogle, JPTooglelist and JPExposeButton actuate from inside draw(),
// so no amount of click ordering could have fixed those - only a test they
// themselves consult.
//
// This is deliberately a static, opt-in layer stack rather than a parameter
// threaded through every control: with NO layer pushed, available() is always
// true, so any code path that has not been scoped behaves exactly as before.
// That keeps the blast radius of teaching JPdragobject::mouseOver() about
// occlusion down to the scopes that actually opt in.
namespace jp_pointer
{
	// Layer orders. ofApp's SurfaceId is defined from these so the z-order has
	// one set of numbers rather than two that can drift apart.
	constexpr int kNone = -1;   // nothing scoped: never occluded
	constexpr int kCanvas = 0;
	constexpr int kInspector = 10;
	constexpr int kCuePanel = 20;
	constexpr int kMappingPanel = 30;
	constexpr int kShaderEditor = 40;
	constexpr int kFieldEdit = 60;
	// The MIDI panel body sits just below its own dropdowns.
	constexpr int kMidiBody = 65;
	constexpr int kDropdown = 70;
	// A prompt drawn INSIDE a panel still has to own the pointer above that
	// panel's own body, or the modal rule below blocks its own buttons.
	constexpr int kPrompt = 90;
	constexpr int kModal = 100;

	namespace detail
	{
		// (x, y, layerOrder) -> is something ABOVE that layer covering the point
		inline std::function<bool(float, float, int)> &occlusionTest()
		{
			static std::function<bool(float, float, int)> fn;
			return fn;
		}
		// A single current layer, not a stack. The canvas layer coincides
		// exactly with JPdragobject's mouse override, which is set and cleared
		// an unequal number of times (extra clears are harmless no-ops), so a
		// stack would drift. Assignment is idempotent; Scope saves/restores.
		inline int &currentLayer()
		{
			static int layer = kNone;
			return layer;
		}
	}

	inline void setOcclusionTest(std::function<bool(float, float, int)> fn)
	{
		detail::occlusionTest() = std::move(fn);
	}

	inline void setLayer(int order) { detail::currentLayer() = order; }
	inline int layer() { return detail::currentLayer(); }
	inline void clearLayer() { detail::currentLayer() = kNone; }

	// Screen coordinates, always. JPdragobject's mouse override is a canvas
	// space transform, not a z-order, so occlusion must not go through it.
	inline bool available(float screenX, float screenY)
	{
		const int order = detail::currentLayer();
		if (order == kNone) return true;
		if (!detail::occlusionTest()) return true;
		return !detail::occlusionTest()(screenX, screenY, order);
	}

	inline bool available()
	{
		return available((float)ofGetMouseX(), (float)ofGetMouseY());
	}

	struct Scope
	{
		explicit Scope(int order) : previous(detail::currentLayer())
		{
			detail::currentLayer() = order;
		}
		~Scope() { detail::currentLayer() = previous; }
		Scope(const Scope &) = delete;
		Scope &operator=(const Scope &) = delete;
	private:
		int previous;
	};
}
