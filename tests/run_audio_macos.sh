#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p obj/audio-macos
xcrun clang++ -std=c++17 -O2 -pthread tests/audio_core_tests.cpp src/JPutils/jp_audio_analyzer.cpp -o obj/audio-macos/audio_core_tests
obj/audio-macos/audio_core_tests
bundle="obj/audio-macos/GuipperAudioTest.app"
mkdir -p "$bundle/Contents/MacOS"
cp tests/audio_macos_test.plist "$bundle/Contents/Info.plist"
xcrun clang++ -std=c++17 -O2 -fobjc-arc -mmacosx-version-min=11.0 \
    tests/audio_loopback_macos.mm src/JPutils/jp_audio_loopback_macos.mm \
    -framework CoreAudio -framework Foundation -o "$bundle/Contents/MacOS/GuipperAudioTest"
codesign --force --sign - "$bundle"
"$bundle/Contents/MacOS/GuipperAudioTest" "$@"
