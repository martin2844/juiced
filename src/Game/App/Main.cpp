// OpenJuice: an engine that runs Juiced (2005) from the files of an installed copy of the game.
// The startup follows the stock game step by step (docs/spec/01-startup.md).
//
// Switches of OpenJuice (besides those of the game; Game/Platform/CommandLine.h):
//   -setup       the game's configuration tool only
// and in the debug builds only, among others:
//   --frames N   quit after N rendered frames
//   --track <Track>/<Route>[/<time>[/<weather>]]   a view of a route instead of the interface
//   --test       hidden window, windowed, no change to the system's accessibility keys, no configuration tool

#include "Core/Base/Config.h"
#include "Core/Base/Cpu.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Race/Racer.h"
#include "Core/Saves/SaveEntries.h"
#include "Game/App/CrashLog.h"
#include "Game/App/GameLoop.h"
#include "Game/App/Version.h"
#include "Game/Cars/CarSound.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/Platform.h"
#include "Game/Platform/Settings.h"
#include "Game/Platform/SystemKeys.h"
#include "Game/Race/Hud/RaceWindows.h"

namespace {

using namespace app;

// Worker threads of WinMain: the processors of the system affinity mask (at most 2) are counted and the main thread
// is bound to processor 0; with threading on and 2 processors the owned mutex JUICEDMUTEX is created.
class Threads {
public:
    explicit Threads(uint32_t threading) {
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
            for (int bit = 0; bit < 32; ++bit) {
                if (systemMask & (DWORD_PTR{1} << bit)) {
                    ++m_count;
                }
            }
        }
        m_count = m_count > 2 ? 2 : m_count;
        SetThreadAffinityMask(GetCurrentThread(), 1);
        if (threading == 0) {
            m_count = 1;
        }
        if (m_count > 1) {
            m_mutex = CreateMutexA(nullptr, TRUE, "JUICEDMUTEX");
        }
    }
    ~Threads() {
        if (m_mutex != nullptr) {
            CloseHandle(m_mutex);
        }
    }
    uint32_t count() const { return m_count; }

private:
    uint32_t m_count = 0;
    HANDLE m_mutex = nullptr;
};

// 0x005A3740: the game's main function (0x00418300) run under the filter of crash.txt.
int runMain(const oj::LaunchOptions& options, const oj::Config& config) {
    SetUnhandledExceptionFilter(writeCrashTxt);
    return gameMain(options, config);
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int) {
    const std::filesystem::path folder = executableFolder();
    oj::log::open(folder / L"OpenJuice.log");
    oj::log::info(OJ_PRODUCT_TEXT "." OJ_RELEASE_TEXT ", " OJ_BASED_ON_TEXT " (" OJ_BUILD_TEXT ", " OJ_RELEASE_NAME
                  ")");
    SetUnhandledExceptionFilter(logCrash);

    const oj::Config config = oj::Config::load(folder / L"OpenJuice.ini");
    const oj::LaunchOptions options = oj::parseCommandLine(commandLine);
    // The stock executable runs from the game's folder only: away from it the system does not find the game's
    // binkw32.dll it imports. OpenJuice, which can do without it, says so instead. The game's files are then read
    // from the current directory, as the stock game reads them.
    if (!oj::GameFiles(folder).valid()) {
        oj::log::error("the executable's folder is not a Juiced folder ({})", oj::toUtf8(folder.wstring()));
        if (!options.test) {
            MessageBoxW(nullptr, L"The game's folder was not found: put the executable in the folder of Juiced.",
                        L"Juiced", MB_OK);
        }
        oj::log::close();
        return 0;
    }
    const oj::GameFiles game(std::filesystem::current_path());
    if (game.valid()) {
        oj::log::info("game: {}", oj::toUtf8(game.root().wstring()));
    } else {
        oj::log::warning("the current directory is not a Juiced folder ({})", oj::toUtf8(game.root().wstring()));
    }
    GetCurrentDirectoryA(sizeof g_gameFolder, g_gameFolder);

    // 0x005A3CD2: the command line split into words, to the debugger's output.
    oj::log::debugPrint("pre if(vArgs.size() == 0)\n");
    const uint32_t features = oj::cpu::detectFeatures();
    oj::log::info("cpu features 0x{:X}", features);

    int code = 0;
    {
        oj::SystemKeys keys(!options.test);
        const Threads threads(oj::Settings{}.threading);
        oj::log::info("{} processor(s) for the game", threads.count());
        g_gameProcessors = threads.count();
        code = runMain(options, config);
    }
    oj::log::close();
    return code;
}
