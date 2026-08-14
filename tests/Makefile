CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror
AUDIO_TARGET := audio_core_tests
AUDIO_SOURCES := audio_core_tests.cpp ../src/JPutils/jp_audio_analyzer.cpp
MEDIA_TARGET := media_core_tests
MEDIA_SOURCES := media_core_tests.cpp
TARGETS := $(AUDIO_TARGET) $(MEDIA_TARGET)

.PHONY: all run tsan clean

all: $(TARGETS)

$(AUDIO_TARGET): $(AUDIO_SOURCES)
	$(CXX) $(CXXFLAGS) -pthread $(AUDIO_SOURCES) -o $@

$(MEDIA_TARGET): $(MEDIA_SOURCES)
	$(CXX) $(CXXFLAGS) $(MEDIA_SOURCES) -o $@

run: $(TARGETS)
	./$(AUDIO_TARGET)
	./$(MEDIA_TARGET)

tsan:
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread \
		-fsanitize=thread $(AUDIO_SOURCES) -o $(AUDIO_TARGET)_tsan
	setarch $$(uname -m) -R ./$(AUDIO_TARGET)_tsan

clean:
	rm -f $(TARGETS) $(AUDIO_TARGET)_tsan
