#pragma once
// OpenJuice's version: the retail build of the game this branch follows (1.00, or 1.01 with the official patch) and
// OpenJuice's own release of it. The executable's version resource (OpenJuice.rc) and the log's first line show it.

#define OJ_GAME_VERSION_MAJOR 1
#define OJ_GAME_VERSION_MINOR 1
#define OJ_GAME_VERSION_TEXT "1.01"

#define OJ_RELEASE 2
#define OJ_RELEASE_TEXT "2"
#define OJ_RELEASE_NAME "Bug fixes"

// The resource's version: the game's build, then the release ("1.01.2": Juiced 1.01, OpenJuice's release 2).
#define OJ_VERSION_TEXT OJ_GAME_VERSION_TEXT "." OJ_RELEASE_TEXT

#if defined(_WIN64) || defined(OJ_RC_X64)
#define OJ_PLATFORM_TEXT "64-bit"
#else
#define OJ_PLATFORM_TEXT "32-bit"
#endif

#if defined(_DEBUG) || defined(OJ_RC_DEBUG)
#define OJ_BUILD_TEXT OJ_PLATFORM_TEXT ", debug"
#else
#define OJ_BUILD_TEXT OJ_PLATFORM_TEXT
#endif
