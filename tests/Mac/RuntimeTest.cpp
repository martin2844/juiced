#include "Mods/Runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oj::mods::runtime;
void check(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
int main() {
    auto dir = std::filesystem::temp_directory_path() / ("juiced-runtime-" + std::to_string(getpid()));
    try {
        std::filesystem::create_directories(dir / "example");
        auto script = dir / "example/main.lua";
        std::ofstream(script) << "return {api=1, neon_colour=function(rgb,e,s,t) return 123 end}";
        load(dir);
        check(number("neon_colour", {0., 0., 0., 0.}, 0) == 123, "external script executes");
        std::ofstream(script) << "return {api=1, neon_colour=function(rgb,e,s,t) return 456 end}";
        load(dir);
        check(number("neon_colour", {0., 0., 0., 0.}, 0) == 456, "editing Lua changes behaviour without rebuilding");
        std::ofstream(dir / "example/disabled") << "";
        load(dir);
        check(loaded().empty() && number("neon_colour", {}, 999) == 999, "disabled mod preserves host fallback");
        std::filesystem::remove(dir / "example/disabled");
        std::ofstream(dir / "bad.lua") << "not valid Lua !";
        std::ofstream(dir / "wrong-api.lua") << "return {api=300}";
        std::ofstream(dir / "failure.lua") << "return {api=1, broken=function() error('expected failure') end}";
        std::ofstream(dir / "loop.lua") << "return {api=1, looping=function() while true do end end}";
        load(dir);
        check(number("broken", {}, 7) == 7, "callback exception falls back");
        check(number("looping", {}, 8) == 8, "runaway callback hits instruction limit");
        check(number("neon_colour", {}, 0) == 456 && loaded().size() == 1, "broken mods isolated from healthy mod");
        std::ofstream(script) << "return {api=1, isolated=function() return os==nil and io==nil and package==nil and "
                                 "debug==nil and load==nil end}";
        load(dir);
        check(call("isolated", {}, false) == Value(true), "scripts have no shell or file access");
        load({});
        check(!feature("neon"), "no mods means no injected feature");
        std::filesystem::remove_all(dir);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(dir);
        return 1;
    }
}
