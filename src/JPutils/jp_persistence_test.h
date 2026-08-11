#pragma once

class ofApp;

namespace jp_persistence_test
{
	// Runs only when GUIPPER_PERSISTENCE_TEST is set. Returns false on any
	// schema, ordering or clamping regression.
	bool run(ofApp &app);
}
