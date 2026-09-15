#pragma once
#ifndef GUIPPER_VERSION
#define GUIPPER_VERSION "0.1.0-beta.2"
#endif
#define GUIPPER_WIDEN_INNER(x) L##x
#define GUIPPER_WIDEN(x) GUIPPER_WIDEN_INNER(x)
#define GUIPPER_WIDE_VERSION GUIPPER_WIDEN(GUIPPER_VERSION)
namespace jp { inline constexpr const char* version = GUIPPER_VERSION; }
