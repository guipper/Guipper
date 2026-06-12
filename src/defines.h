#pragma once


#define NDI

// Spout is a Windows-only (DirectX) texture-sharing API. Only enable it when
// building for Windows; on Linux/macOS the Spout box and SpoutSDK are compiled
// out so the project builds with NDI/OSC/MIDI only.
#if defined(_WIN32)
#define SPOUT
#endif

#define RELATIVEDIRS
