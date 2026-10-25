#pragma once
// OpenJuice's version: the retail build of the game this branch follows (1.00, or 1.01 with the official patch) and
// OpenJuice's own release of it. The executable's version resource (OpenJuice.rc) and the log's first line show it.

#define OJ_GAME_VERSION_MAJOR 1
#define OJ_GAME_VERSION_MINOR 1
#define OJ_GAME_VERSION_TEXT "1.01"

#define OJ_RELEASE 5
#define OJ_RELEASE_TEXT "5"
#define OJ_RELEASE_NAME "Bug fixes"

// The resource's version: the game's build, then the release ("1.01.5": Juiced 1.01, OpenJuice's release 5).
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

// The unofficial patch (the branch unofficial-patch/1.02/<API>: the retail build of Juiced 1.01 with the stock game's
// problems fixed, docs/fixes.md): its version "1.02.<the release>" ("1.02.5"; the release the retail branches', the
// patch made again with each), in the resource (its numbers 1.2.0.<the release>) and the log's first line; the game it
// plays is still Juiced 1.01. Kept here, apart from the release's lines above, so that the retail branches' release
// changes rebase cleanly.
#define OJ_PATCH_VERSION_TEXT "1.02"
#undef OJ_GAME_VERSION_MINOR
#define OJ_GAME_VERSION_MINOR 2
#undef OJ_VERSION_TEXT
#define OJ_VERSION_TEXT OJ_PATCH_VERSION_TEXT "." OJ_RELEASE_TEXT
// Its names: the resource's description "OpenJuice unofficial patch 1.02 for Juiced (64-bit)", its product version
// "1.02.5 (based on Juiced 1.01, Bug fixes)", the log's first line "OpenJuice unofficial patch 1.02.5, based on Juiced
// 1.01 (64-bit, Bug fixes)".
#define OJ_PRODUCT_TEXT "OpenJuice unofficial patch " OJ_PATCH_VERSION_TEXT
#define OJ_BASED_ON_TEXT "based on Juiced " OJ_GAME_VERSION_TEXT
