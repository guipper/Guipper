#pragma once

// How many media FBO passes a frame actually performed, and how many it skipped.
//
// The skip is invisible by construction - that is the point of it - which means
// a later edit that accidentally invalidates the render signature every frame
// would silently undo the optimisation and nothing would look wrong. These two
// counters are what make that visible on the debug overlay.
//
// Same shape as jp_pointer: function-local statics behind inline accessors, so
// it stays header-only and costs an increment. Main thread only; box updates
// and the overlay both run there.
namespace jp_box_media_stats
{
	namespace detail
	{
		inline int &renderedThisFrame() { static int v = 0; return v; }
		inline int &skippedThisFrame() { static int v = 0; return v; }
		inline int &renderedLastFrame() { static int v = 0; return v; }
		inline int &skippedLastFrame() { static int v = 0; return v; }
	}

	inline void countRendered() { ++detail::renderedThisFrame(); }
	inline void countSkipped() { ++detail::skippedThisFrame(); }

	// Call once per frame before the graph update. Publishes the completed
	// frame's totals so the overlay reads a whole frame rather than a partial
	// one that depends on where in the box list drawing happened to be.
	inline void beginFrame()
	{
		detail::renderedLastFrame() = detail::renderedThisFrame();
		detail::skippedLastFrame() = detail::skippedThisFrame();
		detail::renderedThisFrame() = 0;
		detail::skippedThisFrame() = 0;
	}

	inline int getRendered() { return detail::renderedLastFrame(); }
	inline int getSkipped() { return detail::skippedLastFrame(); }
}
