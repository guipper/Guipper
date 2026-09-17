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
UNIFORM_TARGET := uniform_parser_tests
UNIFORM_SOURCES := uniform_parser_tests.cpp ../src/JPutils/jp_uniform_parser.cpp
UNIFORM_HEADERS := ../src/JPutils/jp_uniform_parser.h
TARGETS := review_tests toast_tests shader_catalog_tests appimage_backend_tests update_service_tests app_paths_tests $(UNIFORM_TARGET) $(AUDIO_TARGET) $(MEDIA_TARGET) $(PAINT_TARGET) $(GRAPH_TARGET) $(WRAP_TARGET) $(KEYMAP_TARGET) $(OSC_TARGET) $(EDITOR_SHORTCUT_TARGET) $(MAPPING_BOOLEAN_TARGET)

.PHONY: all run tsan clean

all: $(TARGETS)

toast_tests: toast_tests.cpp ../src/JPutils/jp_toast.h
	$(CXX) $(CXXFLAGS) toast_tests.cpp -o $@

JSON_INCLUDE ?= ../../../../libs/json/include
review_tests: review_tests.cpp ../src/JPutils/jp_review.cpp ../src/JPutils/jp_review.h ../src/JPutils/jp_app_paths.cpp
	$(CXX) $(CXXFLAGS) -I$(JSON_INCLUDE) review_tests.cpp ../src/JPutils/jp_review.cpp ../src/JPutils/jp_app_paths.cpp -o $@

shader_catalog_tests: shader_catalog_tests.cpp ../src/JPutils/jp_shader_catalog.h
	$(CXX) $(CXXFLAGS) -I$(JSON_INCLUDE) shader_catalog_tests.cpp -o $@

appimage_backend_tests: appimage_backend_tests.cpp ../src/JPutils/jp_update_platform.cpp ../src/JPutils/jp_update_service.cpp ../src/JPutils/jp_app_paths.cpp
	$(CXX) $(CXXFLAGS) -pthread -DGUIPPER_UPDATE_TEST -DGUIPPER_APPIMAGE_UPDATES -DGUIPPER_APPIMAGE_STABLE='"test-stable"' -DGUIPPER_APPIMAGE_BETA='"test-beta"' appimage_backend_tests.cpp ../src/JPutils/jp_update_platform.cpp ../src/JPutils/jp_update_service.cpp ../src/JPutils/jp_app_paths.cpp -o $@

update_service_tests: update_service_tests.cpp ../src/JPutils/jp_update_service.cpp ../src/JPutils/jp_update_service.h
	$(CXX) $(CXXFLAGS) update_service_tests.cpp ../src/JPutils/jp_update_service.cpp -o $@

app_paths_tests: app_paths_tests.cpp ../src/JPutils/jp_app_paths.cpp ../src/JPutils/jp_app_paths.h
	$(CXX) $(CXXFLAGS) app_paths_tests.cpp ../src/JPutils/jp_app_paths.cpp -o $@

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

$(UNIFORM_TARGET): $(UNIFORM_SOURCES) $(UNIFORM_HEADERS)
	$(CXX) $(CXXFLAGS) $(UNIFORM_SOURCES) -o $@

run: $(TARGETS)
	./review_tests
	./toast_tests
	./shader_catalog_tests
	./appimage_backend_tests
	./update_service_tests
	./app_paths_tests
	./$(UNIFORM_TARGET)
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
