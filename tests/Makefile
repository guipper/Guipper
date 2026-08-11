CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror
TARGET := audio_core_tests
SOURCES := audio_core_tests.cpp ../src/JPutils/jp_audio_analyzer.cpp

.PHONY: all run tsan clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -pthread $(SOURCES) -o $@

run: $(TARGET)
	./$(TARGET)

tsan:
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread \
		-fsanitize=thread $(SOURCES) -o $(TARGET)_tsan
	setarch $$(uname -m) -R ./$(TARGET)_tsan

clean:
	rm -f $(TARGET) $(TARGET)_tsan
