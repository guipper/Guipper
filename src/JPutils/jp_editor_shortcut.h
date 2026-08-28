#pragma once

namespace jp_editor_shortcut
{
	enum class Action
	{
		None,
		SelectAll,
		Copy,
		Cut,
		Paste
	};

	// Resolve from the detailed key event. GLFW letter keycodes use uppercase
	// ASCII values on every desktop platform; `key` may instead carry either a
	// printable letter or the folded Ctrl control code.
	inline Action resolve(int key, int keycode, bool control, bool command)
	{
		auto chord = [&](char upper, char lower, int controlCode)
		{
			return key == controlCode || ((control || command) &&
				(keycode == upper || key == lower || key == upper));
		};
		if (chord('A', 'a', 1)) return Action::SelectAll;
		if (chord('C', 'c', 3)) return Action::Copy;
		if (chord('X', 'x', 24)) return Action::Cut;
		if (chord('V', 'v', 22)) return Action::Paste;
		return Action::None;
	}
}
