// The tests' runner.
//   OpenJuice.Tests.exe [save folder] [--game <folder>] [--unit | --integration] [--suite <name>] [--list]
// The save folder is that of the player's saves the integration tests read and write back (default
// %APPDATA%\THQ\Juiced); the game's data files are read from the --game folder, an installed Juiced (without one the
// tests of the game's data are left out).
// The unit tests run first, then the integration tests; in each, the suites in the order of their names.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/Text.h"
#include "Core/Saves/Save.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

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
        } else {
            root = a;
        }
    }
    oj::test::setSaveRoot(root);

    std::vector<oj::test::TestCase> tests = oj::test::registry();
    std::stable_sort(tests.begin(), tests.end(), [](const oj::test::TestCase& a, const oj::test::TestCase& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        return std::string(a.suite) < std::string(b.suite);
    });
    if (list) {
        for (const oj::test::TestCase& t : tests) {
            std::printf("%s %s.%s\n", t.kind == oj::test::Kind::Unit ? "unit" : "integration", t.suite, t.name);
        }
        return 0;
    }

    std::printf("save root %s\n", oj::toUtf8(root.wstring()).c_str());
    for (const oj::test::TestCase& t : tests) {
        if ((t.kind == oj::test::Kind::Unit && !unit) || (t.kind == oj::test::Kind::Integration && !integration)) {
            continue;
        }
        if (!suite.empty() && suite != t.suite) continue;
        t.run();
    }
    std::printf("%d passed, %d failed\n", oj::test::passed(), oj::test::failed());
    return oj::test::failed() == 0 ? 0 : 1;
}
