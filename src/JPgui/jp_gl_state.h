#pragma once

#include "ofMain.h"

#include <algorithm>
#include <cmath>

namespace jp_gl
{
	// Owns the raw OpenGL scissor state for one draw scope. Coordinates passed
	// to set() are openFrameworks window coordinates; conversion to framebuffer
	// pixels is derived from the live viewport, so clips remain correct on HiDPI
	// windows and secondary output contexts.
	class ScopedScissor
	{
	public:
		ScopedScissor()
		{
			wasEnabled = glIsEnabled(GL_SCISSOR_TEST);
			glGetIntegerv(GL_SCISSOR_BOX, previousBox);
			glGetIntegerv(GL_VIEWPORT, viewport);
		}

		explicit ScopedScissor(const ofRectangle &bounds)
			: ScopedScissor()
		{
			set(bounds);
		}

		~ScopedScissor()
		{
			if (wasEnabled)
			{
				glEnable(GL_SCISSOR_TEST);
				glScissor(previousBox[0], previousBox[1],
					previousBox[2], previousBox[3]);
			}
			else
			{
				glDisable(GL_SCISSOR_TEST);
			}
		}

		ScopedScissor(const ScopedScissor &) = delete;
		ScopedScissor &operator=(const ScopedScissor &) = delete;

		void set(const ofRectangle &bounds)
		{
			const float logicalW = std::max(1.0f, (float)ofGetWidth());
			const float logicalH = std::max(1.0f, (float)ofGetHeight());
			const float scaleX = viewport[2] / logicalW;
			const float scaleY = viewport[3] / logicalH;
			glEnable(GL_SCISSOR_TEST);
			glScissor(
				viewport[0] + (GLint)std::floor(bounds.x * scaleX),
				viewport[1] + (GLint)std::floor(
					(logicalH - bounds.getBottom()) * scaleY),
				std::max(0, (GLint)std::ceil(bounds.width * scaleX)),
				std::max(0, (GLint)std::ceil(bounds.height * scaleY)));
		}

		void setFullViewport()
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(viewport[0], viewport[1], viewport[2], viewport[3]);
		}

	private:
		GLboolean wasEnabled = GL_FALSE;
		GLint previousBox[4] = {0, 0, 0, 0};
		GLint viewport[4] = {0, 0, 0, 0};
	};

	inline void resetWindowDrawState(float width, float height)
	{
		// A box render is allowed to use arbitrary FBO dimensions. Window draw
		// callbacks are context boundaries, so never trust the framebuffer left
		// current by the last offscreen pass (framebuffer 0 is per GL context and
		// therefore also correct for each secondary output window).
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_SCISSOR_TEST);
		ofViewport(0.0f, 0.0f, width, height, false);
		ofSetupScreen();
		ofEnableAlphaBlending();
		ofSetColor(255, 255, 255, 255);
		ofSetRectMode(OF_RECTMODE_CORNER);
	}
}
