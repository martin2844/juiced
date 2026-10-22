#include "Mods/Runtime.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
using namespace oj::mods::runtime;
void check(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
// A regression in Lua error containment must fail this test, not kill or wedge CTest.
void isolated(const std::filesystem::path &root, const char *label, const std::string &script,
              const std::function<void()> &exercise) {
    const auto dir = root / "isolated";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "case.lua") << script;
    std::ofstream(dir / "healthy.lua") << "return {api=1, healthy=function() return 77 end}";
    std::cout.flush();
    const pid_t child = fork();
    check(child >= 0, "spawn isolated runtime case");
    if (child == 0) {
        alarm(5);
        try {
            load(dir);
            exercise();
            check(number("healthy", {}, 0) == 77, "healthy mod survives");
            load({});
            _exit(0);
        } catch (const std::exception &e) {
            std::cerr << label << ": " << e.what() << std::endl;
            _exit(1);
        }
    }
    int status = 0;
    check(waitpid(child, &status, 0) == child, "join isolated runtime case");
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, label);
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
        for (const char *error : {"nil", "{}", "false"}) {
            isolated(dir, "non-string startup error is contained", std::string("error(") + error + ")",
                     [] { check(loaded().size() == 1, "bad startup mod skipped"); });
            isolated(dir, "non-string callback error is contained",
                     std::string("return {api=1,probe=function() error(") + error + ") end}",
                     [] { check(number("probe", {}, 42) == 42, "bad callback falls back"); });
        }
        isolated(dir, "API metatable cannot run during validation",
                 "return setmetatable({}, {__index=function() error('API lookup') end})",
                 [] { check(loaded().size() == 1, "raw API validation"); });
        isolated(dir, "hook metatable cannot execute outside protection",
                 "return setmetatable({api=1},{__index=function() while true do end end})",
                 [] { check(number("probe", {}, 42) == 42, "raw hook lookup"); });
        isolated(dir, "menu metatable cannot execute outside protection",
                 "return {api=1,image_menu=function() return {setmetatable({},{__index=function() error('menu lookup') "
                 "end})} end}",
                 [] { check(imageMenu(false, false, false).empty(), "raw menu fields"); });
        isolated(dir, "pcall cannot catch host execution limit",
                 "return {api=1,probe=function() while true do pcall(function() while true do end end) end end}",
                 [] { check(number("probe", {}, 42) == 42 && loaded().size() == 1, "runaway pcall disabled"); });
        isolated(dir, "xpcall cannot catch host execution limit",
                 "return {api=1,probe=function() while true do xpcall(function() while true do end end,function(e) "
                 "return e end) end end}",
                 [] { check(number("probe", {}, 42) == 42 && loaded().size() == 1, "runaway xpcall disabled"); });
        isolated(dir, "ordinary protected calls retain Lua semantics",
                 "return {api=1,probe=function() local ok,value=pcall(function() return 12 end);local "
                 "failed,message=xpcall(function() error('test') end,function() return 'caught' end);return ok and "
                 "value==12 and not failed and message=='caught' end}",
                 [] { check(call("probe", {}, false) == Value(true), "ordinary pcall and xpcall work"); });
        isolated(dir, "oversized arguments fail inside protected boundary",
                 "return {api=1,probe=function() return 123 end}", [] {
                     check(number("probe", {std::string(20 * 1024 * 1024, 'x')}, 42) == 42 && loaded().size() == 1,
                           "argument allocation failure contained");
                 });
        isolated(dir, "script finalizers rejected before registration",
                 "return setmetatable({api=1},{__gc=function() while true do pcall(function() while true do end end) "
                 "end end})",
                 [] { check(loaded().size() == 1, "finalizer rejected during mod load"); });
        std::filesystem::remove_all(dir);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(dir);
        return 1;
    }
}
