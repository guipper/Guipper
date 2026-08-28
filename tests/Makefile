CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror
AUDIO_TARGET := audio_core_tests
AUDIO_SOURCES := audio_core_tests.cpp ../src/JPutils/jp_audio_analyzer.cpp
AUDIO_HEADERS := ../src/JPutils/jp_audio_analyzer.h
MEDIA_TARGET := media_core_tests
MEDIA_SOURCES := media_core_tests.cpp
MEDIA_HEADERS := ../src/JPbox/jp_media_state.h
PAINT_TARGET := paint_core_tests
PAINT_SOURCES := paint_core_tests.cpp
PAINT_HEADERS := ../src/JPbox/jp_paint_doc.h ../src/JPbox/jp_media_state.h
GRAPH_TARGET := graph_history_tests
GRAPH_SOURCES := graph_history_tests.cpp
GRAPH_HEADERS := ../src/JPutils/jp_graph_history.h
WRAP_TARGET := textwrap_tests
WRAP_SOURCES := textwrap_tests.cpp
WRAP_HEADERS := ../src/JPutils/jp_textwrap.h
KEYMAP_TARGET := keymap_tests
KEYMAP_SOURCES := keymap_tests.cpp
KEYMAP_HEADERS := ../src/JPutils/jp_keymap.h ../src/JPutils/jp_help_content.h
OSC_TARGET := osc_address_tests
OSC_SOURCES := osc_address_tests.cpp
OSC_HEADERS := ../src/JPutils/jp_osc_address.h
EDITOR_SHORTCUT_TARGET := editor_shortcut_tests
EDITOR_SHORTCUT_SOURCES := editor_shortcut_tests.cpp
EDITOR_SHORTCUT_HEADERS := ../src/JPutils/jp_editor_shortcut.h
MAPPING_BOOLEAN_TARGET := mapping_boolean_tests
MAPPING_BOOLEAN_SOURCES := mapping_boolean_tests.cpp
MAPPING_BOOLEAN_HEADERS := ../src/JPbox/jp_mapping_boolean.h
TARGETS := $(AUDIO_TARGET) $(MEDIA_TARGET) $(PAINT_TARGET) $(GRAPH_TARGET) $(WRAP_TARGET) $(KEYMAP_TARGET) $(OSC_TARGET) $(EDITOR_SHORTCUT_TARGET) $(MAPPING_BOOLEAN_TARGET)

.PHONY: all run tsan clean

all: $(TARGETS)

$(AUDIO_TARGET): $(AUDIO_SOURCES) $(AUDIO_HEADERS)
	$(CXX) $(CXXFLAGS) -pthread $(AUDIO_SOURCES) -o $@

$(MEDIA_TARGET): $(MEDIA_SOURCES) $(MEDIA_HEADERS)
	$(CXX) $(CXXFLAGS) $(MEDIA_SOURCES) -o $@

$(PAINT_TARGET): $(PAINT_SOURCES) $(PAINT_HEADERS)
	$(CXX) $(CXXFLAGS) $(PAINT_SOURCES) -o $@

$(GRAPH_TARGET): $(GRAPH_SOURCES) $(GRAPH_HEADERS)
	$(CXX) $(CXXFLAGS) $(GRAPH_SOURCES) -o $@

$(WRAP_TARGET): $(WRAP_SOURCES) $(WRAP_HEADERS)
	$(CXX) $(CXXFLAGS) $(WRAP_SOURCES) -o $@

$(KEYMAP_TARGET): $(KEYMAP_SOURCES) $(KEYMAP_HEADERS)
	$(CXX) $(CXXFLAGS) $(KEYMAP_SOURCES) -o $@

$(OSC_TARGET): $(OSC_SOURCES) $(OSC_HEADERS)
	$(CXX) $(CXXFLAGS) $(OSC_SOURCES) -o $@

$(EDITOR_SHORTCUT_TARGET): $(EDITOR_SHORTCUT_SOURCES) $(EDITOR_SHORTCUT_HEADERS)
	$(CXX) $(CXXFLAGS) $(EDITOR_SHORTCUT_SOURCES) -o $@

$(MAPPING_BOOLEAN_TARGET): $(MAPPING_BOOLEAN_SOURCES) $(MAPPING_BOOLEAN_HEADERS)
	$(CXX) $(CXXFLAGS) $(MAPPING_BOOLEAN_SOURCES) -o $@

run: $(TARGETS)
	./$(AUDIO_TARGET)
	./$(MEDIA_TARGET)
	./$(PAINT_TARGET)
	./$(GRAPH_TARGET)
	./$(WRAP_TARGET)
	./$(KEYMAP_TARGET)
	./$(OSC_TARGET)
	./$(EDITOR_SHORTCUT_TARGET)
	./$(MAPPING_BOOLEAN_TARGET)

tsan:
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread \
		-fsanitize=thread $(AUDIO_SOURCES) -o $(AUDIO_TARGET)_tsan
	setarch $$(uname -m) -R ./$(AUDIO_TARGET)_tsan

clean:
	rm -f $(TARGETS) $(AUDIO_TARGET)_tsan
