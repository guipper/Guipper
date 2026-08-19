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
TARGETS := $(AUDIO_TARGET) $(MEDIA_TARGET) $(PAINT_TARGET)

.PHONY: all run tsan clean

all: $(TARGETS)

$(AUDIO_TARGET): $(AUDIO_SOURCES) $(AUDIO_HEADERS)
	$(CXX) $(CXXFLAGS) -pthread $(AUDIO_SOURCES) -o $@

$(MEDIA_TARGET): $(MEDIA_SOURCES) $(MEDIA_HEADERS)
	$(CXX) $(CXXFLAGS) $(MEDIA_SOURCES) -o $@

$(PAINT_TARGET): $(PAINT_SOURCES) $(PAINT_HEADERS)
	$(CXX) $(CXXFLAGS) $(PAINT_SOURCES) -o $@

run: $(TARGETS)
	./$(AUDIO_TARGET)
	./$(MEDIA_TARGET)
	./$(PAINT_TARGET)

tsan:
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread \
		-fsanitize=thread $(AUDIO_SOURCES) -o $(AUDIO_TARGET)_tsan
	setarch $$(uname -m) -R ./$(AUDIO_TARGET)_tsan

clean:
	rm -f $(TARGETS) $(AUDIO_TARGET)_tsan
