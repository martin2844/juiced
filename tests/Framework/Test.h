#pragma once
// The tests' framework. Each test registers itself with its kind, suite and name (OJ_UNIT_TEST, OJ_INTEGRATION_TEST)
// and reports what it checks (check: PASS or FAIL with the text). The runner (Framework/TestMain.cpp) runs the unit
// tests, then the integration tests, and counts the checks.
//   Unit tests: one part of the engine with the data they make themselves.
//   Integration tests: parts working together through the file system, sockets on the loopback, the player's saves
//   or the installed game's files (Framework/Fixtures.h).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oj::test {

enum class Kind { Unit, Integration };

struct TestCase {
    Kind kind;
    const char* suite;
    const char* name;
    void (*run)();
};

// Every test, in the order the program made them.
std::vector<TestCase>& registry();

struct Registrar {
    Registrar(Kind kind, const char* suite, const char* name, void (*run)()) {
        registry().push_back({kind, suite, name, run});
    }
};

// A check's outcome: printed (PASS or FAIL, then the text) and counted.
void check(bool ok, const std::string& what);
int passed();
int failed();

// A file's bytes as unsigned bytes.
std::vector<uint8_t> bytesOf(const std::vector<std::byte>& file);

}  // namespace oj::test

#define OJ_TEST_DEFINE(kind, suite, name)                                                                     \
    static void suite##_##name##_run();                                                                       \
    static const ::oj::test::Registrar suite##_##name##_registrar(kind, #suite, #name, &suite##_##name##_run); \
    static void suite##_##name##_run()

// A test of one part of the engine.
#define OJ_UNIT_TEST(suite, name) OJ_TEST_DEFINE(::oj::test::Kind::Unit, suite, name)
// A test of parts together, or against files or sockets of the system.
#define OJ_INTEGRATION_TEST(suite, name) OJ_TEST_DEFINE(::oj::test::Kind::Integration, suite, name)
