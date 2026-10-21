#pragma once
// The game's main function and its loop: the platform, the front end, the race and the frames between them
// (docs/spec/01-startup.md).

#include "Core/Base/Config.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/Platform.h"
#include "Game/Platform/Settings.h"

#include <cstdint>
#include <filesystem>

namespace app {

// [0x0079E5C0]: the processors for the game (WinMain's count, at most 2; 1 with threading off).
inline uint32_t g_gameProcessors = 1;

// The folder of the executable (OpenJuice.ini and OpenJuice.log are there).
std::filesystem::path executableFolder();

// The game's main function: random seed, the carmodels.dat check, the game object.
int gameMain(const oj::LaunchOptions& options, const oj::Config& config);

}  // namespace app
