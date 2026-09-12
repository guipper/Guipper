#pragma once

class ofXml;
class JPParameterGroup;

// XML field ownership lives here, independently of graph/UI/cue state.
// These entry points borrow both arguments; no pointers escape the call.
namespace jp_parameter_xml
{
	// Composition files use name-first lookup; presets and clipboard retain
	// their legacy positional behavior. Missing optional fields keep defaults.
	enum class LoadContext { Composition, Preset, Clipboard };
	void save(ofXml &boxNode, JPParameterGroup &group);
	void load(const ofXml &boxNode, JPParameterGroup &group, LoadContext context);
}
