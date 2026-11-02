// The tests' runner.
//   OpenJuice.Tests.exe [save folder] [--game <folder>] [--unit | --integration] [--suite <name>] [--list] [--times]
// The save folder is that of the player's saves the integration tests read and write back (default
// %APPDATA%\THQ\Juiced); the game's data files are read from the --game folder, an installed Juiced (without one the
// tests of the game's data are left out).
// The unit tests run first, then the integration tests; in each, the suites in the order of their names. --times
// prints the tests' times at the end, the slowest first.
// A crash (an exception no one handles, on any thread) writes the test running, the exception and the stack with
// the functions' names and lines to stderr, the output printed so far flushed first.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/Text.h"
#include "Core/Saves/Save.h"

#include <windows.h>
#if defined(_WIN32)
#include <dbghelp.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

namespace {

// The test running ("suite.name"), empty before the first and after the last.
char g_running[256] = {};

#if defined(_WIN32)
void report(const char* text) {
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

LONG WINAPI crashReport(EXCEPTION_POINTERS* info) {
    std::fflush(stdout);
    char line[1024];
    std::snprintf(line, sizeof line, "CRASH 0x%08lX at %p during %s (thread %lu)\n",
                  info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
                  g_running[0] != 0 ? g_running : "no test (before the first or after the last)",
                  GetCurrentThreadId());
    report(line);
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    void* frames[64];
    const USHORT count = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256];
    for (USHORT i = 0; i < count; ++i) {
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        const auto address = reinterpret_cast<DWORD64>(frames[i]);
        DWORD64 offset = 0;
        IMAGEHLP_LINE64 where{};
        where.SizeOfStruct = sizeof where;
        DWORD column = 0;
        const bool named = SymFromAddr(process, address, &offset, symbol) != FALSE;
        const bool placed = SymGetLineFromAddr64(process, address, &column, &where) != FALSE;
        std::snprintf(line, sizeof line, "  %2u %p %s+0x%llX %s:%lu\n", static_cast<unsigned>(i), frames[i],
                      named ? symbol->Name : "?", static_cast<unsigned long long>(offset),
                      placed ? where.FileName : "", placed ? where.LineNumber : 0ul);
        report(line);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif
}  // namespace

namespace oj::test {

namespace {

int g_passed = 0;
int g_failed = 0;

}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    (ok ? g_passed : g_failed)++;
}

int passed() { return g_passed; }
int failed() { return g_failed; }

std::vector<uint8_t> bytesOf(const std::vector<std::byte>& file) {
    const auto* p = reinterpret_cast<const uint8_t*>(file.data());
    return {p, p + file.size()};
}

}  // namespace oj::test

int wmain(int argc, wchar_t** argv) {
    bool unit = true;
    bool integration = true;
    bool list = false;
    bool times = false;
    std::string suite;
    std::filesystem::path root = oj::save::root();
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--unit") {
            integration = false;
        } else if (a == L"--integration") {
            unit = false;
        } else if (a == L"--game" && i + 1 < argc) {
            oj::test::setGameRoot(argv[++i]);
        } else if (a == L"--suite" && i + 1 < argc) {
            suite = oj::toUtf8(argv[++i]);
        } else if (a == L"--list") {
            list = true;
        } else if (a == L"--times") {
            times = true;
        } else if (a.starts_with(L"--")) {
            std::fprintf(stderr, "Unknown option or missing argument: %s\n", oj::toUtf8(a).c_str());
            return 2;
        } else {
            root = a;
        }
    }
    oj::test::setSaveRoot(root);
    if (!unit && !integration) {
        std::fprintf(stderr, "--unit and --integration are mutually exclusive\n");
        return 2;
    }

    std::vector<oj::test::TestCase> tests = oj::test::registry();
    std::stable_sort(tests.begin(), tests.end(), [](const oj::test::TestCase& a, const oj::test::TestCase& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        return std::string(a.suite) < std::string(b.suite);
    });
    std::erase_if(tests, [&](const oj::test::TestCase& t) {
        return (t.kind == oj::test::Kind::Unit && !unit)
            || (t.kind == oj::test::Kind::Integration && !integration)
            || (!suite.empty() && suite != t.suite);
    });
    if (tests.empty()) {
        std::fprintf(stderr, "No tests match the requested selection\n");
        return 2;
    }
    if (list) {
        for (const oj::test::TestCase& t : tests) {
            std::printf("%s %s.%s\n", t.kind == oj::test::Kind::Unit ? "unit" : "integration", t.suite, t.name);
        }
        return 0;
    }

#if defined(_WIN32)
    SetUnhandledExceptionFilter(crashReport);
#endif
    std::printf("save root %s\n", oj::toUtf8(root.wstring()).c_str());
    std::vector<std::pair<double, std::string>> spent;
    const auto start = std::chrono::steady_clock::now();
    for (const oj::test::TestCase& t : tests) {
        std::snprintf(g_running, sizeof g_running, "%s.%s", t.suite, t.name);
        const auto before = std::chrono::steady_clock::now();
        t.run();
        spent.emplace_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count(),
                           g_running);
    }
    g_running[0] = 0;
    if (times) {
        std::stable_sort(spent.begin(), spent.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (const auto& [seconds, name] : spent) std::printf("TIME %8.3f s %s\n", seconds, name.c_str());
        std::printf("TIME %8.3f s in all\n",
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
    std::printf("%d passed, %d failed\n", oj::test::passed(), oj::test::failed());
    if (oj::test::passed() + oj::test::failed() == 0) {
        std::fprintf(stderr, "Selected tests executed no checks\n");
        return 2;
    }
    return oj::test::failed() == 0 ? 0 : 1;
}
