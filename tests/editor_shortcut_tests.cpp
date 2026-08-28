#include "../src/JPutils/jp_editor_shortcut.h"

#include <iostream>

int main()
{
	using jp_editor_shortcut::Action;
	using jp_editor_shortcut::resolve;
	bool ok = true;
	auto expect = [&](bool condition, const char *message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		ok = false;
	};

	expect(resolve('c', 'C', false, false) == Action::None,
		"a plain c became a clipboard chord");
	expect(resolve('c', 'C', true, false) == Action::Copy,
		"Ctrl+C was not recognized");
	expect(resolve('C', 'C', false, true) == Action::Copy,
		"Cmd+C was not recognized");
	expect(resolve('a', 'A', false, true) == Action::SelectAll,
		"Cmd+A was not recognized");
	expect(resolve('x', 'X', true, false) == Action::Cut,
		"Ctrl+X was not recognized");
	expect(resolve('v', 'V', false, true) == Action::Paste,
		"Cmd+V was not recognized");
	expect(resolve(3, 0, false, false) == Action::Copy &&
		resolve(24, 0, false, false) == Action::Cut &&
		resolve(22, 0, false, false) == Action::Paste &&
		resolve(1, 0, false, false) == Action::SelectAll,
		"folded Ctrl control codes were not recognized");
	expect(resolve('s', 'S', true, false) == Action::None,
		"the editor action resolver consumed Ctrl+S");

	if (!ok) return 1;
	std::cout << "editor shortcut tests passed\n";
	return 0;
}
