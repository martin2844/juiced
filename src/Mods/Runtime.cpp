#include "Mods/Runtime.h"
#include "Core/Base/Log.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <optional>
namespace oj::mods::runtime {
namespace {
constexpr size_t kMemoryLimit = 16 * 1024 * 1024;
constexpr int kInstructionLimit = 100000;
struct Module;
void budget(lua_State *, lua_Debug *);
struct Module {
    std::string name;
    size_t bytes = 0;
    lua_State *state = nullptr;
    int exports = LUA_NOREF;
    bool failed = false;
    bool exhausted = false;
    ~Module() {
        if (state) {
            // Closing a state can execute script-defined finalizers too.
            exhausted = false;
            lua_sethook(state, budget, LUA_MASKCOUNT, kInstructionLimit);
            lua_close(state);
        }
    }
};
std::vector<std::unique_ptr<Module>> modules;
std::recursive_mutex mutex;
Module &owner(lua_State *state) {
    return **static_cast<Module **>(lua_getextraspace(state));
}
void *allocate(void *context, void *ptr, size_t oldSize, size_t size) {
    auto &m = *static_cast<Module *>(context);
    if (!ptr)
        oldSize = 0;
    if (!size) {
        m.bytes -= oldSize;
        free(ptr);
        return nullptr;
    }
    if (size > kMemoryLimit || m.bytes - oldSize > kMemoryLimit - size)
        return nullptr;
    auto *next = realloc(ptr, size);
    if (next)
        m.bytes = m.bytes - oldSize + size;
    return next;
}
void budget(lua_State *state, lua_Debug *) {
    owner(state).exhausted = true;
    lua_sethook(state, budget, LUA_MASKCOUNT, 1);
    luaL_error(state, "mod callback exceeded instruction budget");
}
int guardedProtectedCall(lua_State *state) {
    if (owner(state).exhausted)
        return luaL_error(state, "mod instruction budget exhausted");
    const int arguments = lua_gettop(state);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, arguments, LUA_MULTRET);
    // Lua pcall/xpcall may catch ordinary errors, but may not reset the host's budget.
    if (owner(state).exhausted)
        return luaL_error(state, "mod instruction budget exhausted");
    return lua_gettop(state);
}
void rawField(lua_State *state, int index, const char *key) {
    index = lua_absindex(state, index);
    lua_pushstring(state, key);
    lua_rawget(state, index);
}
int guardedSetMetatable(lua_State *state) {
    if (!lua_isnoneornil(state, 2)) {
        luaL_checktype(state, 2, LUA_TTABLE);
        for (const char *key : {"__gc", "__close"}) {
            rawField(state, 2, key);
            if (!lua_isnil(state, -1))
                return luaL_error(state, "script finalizers are not supported");
            lua_pop(state, 1);
        }
    }
    lua_settop(state, 2);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, 2, 1);
    return 1;
}
void push(lua_State *state, const Value &v) {
    if (auto *n = std::get_if<double>(&v))
        lua_pushnumber(state, *n);
    else if (auto *s = std::get_if<std::string>(&v))
        lua_pushlstring(state, s->data(), s->size());
    else
        lua_pushboolean(state, std::get<bool>(v));
}
enum class Operation { Load, Call, Menu };
struct Invocation {
    Module &module;
    Operation operation;
    const char *name;
    const std::vector<Value> *arguments = nullptr;
    std::optional<Value> result;
    std::vector<MenuItem> menu;
};
// All allocating Lua API calls and all script execution live inside lua_pcall.
// Native results belong to Invocation: Lua's longjmp must not skip C++ destructors.
int invoke(lua_State *state) {
    auto &context = *static_cast<Invocation *>(lua_touserdata(state, 1));
    if (context.operation == Operation::Load) {
        for (auto lib : {std::pair<const char *, lua_CFunction>{"_G", luaopen_base},
                         {"math", luaopen_math},
                         {"string", luaopen_string},
                         {"table", luaopen_table},
                         {"utf8", luaopen_utf8}}) {
            luaL_requiref(state, lib.first, lib.second, 1);
            lua_pop(state, 1);
        }
        for (auto key : {"dofile", "loadfile", "load", "collectgarbage", "print"}) {
            lua_pushnil(state);
            lua_setglobal(state, key);
        }
        for (auto key : {"pcall", "xpcall"}) {
            lua_getglobal(state, key);
            lua_pushcclosure(state, guardedProtectedCall, 1);
            lua_setglobal(state, key);
        }
        lua_getglobal(state, "setmetatable");
        lua_pushcclosure(state, guardedSetMetatable, 1);
        lua_setglobal(state, "setmetatable");
        if (luaL_loadfilex(state, context.name, "t") != LUA_OK)
            return lua_error(state);
        lua_call(state, 0, 1);
        if (!lua_istable(state, -1))
            return luaL_error(state, "mod must return an API-v1 table");
        rawField(state, -1, "api");
        if (lua_tointeger(state, -1) != 1)
            return luaL_error(state, "unsupported mod API");
        lua_pop(state, 1);
        context.module.exports = luaL_ref(state, LUA_REGISTRYINDEX);
        return 0;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, context.module.exports);
    rawField(state, -1, context.name);
    if (!lua_isfunction(state, -1))
        return 0;
    for (const auto &arg : *context.arguments)
        push(state, arg);
    lua_call(state, int(context.arguments->size()), 1);
    if (context.operation == Operation::Call) {
        switch (lua_type(state, -1)) {
        case LUA_TNUMBER:
            if (std::isfinite(lua_tonumber(state, -1)))
                context.result = double(lua_tonumber(state, -1));
            break;
        case LUA_TSTRING: {
            size_t length = 0;
            const char *text = lua_tolstring(state, -1, &length);
            context.result = std::string(text, length);
            break;
        }
        case LUA_TBOOLEAN:
            context.result = bool(lua_toboolean(state, -1));
            break;
        }
    } else if (lua_istable(state, -1)) {
        const int count = int(std::min<size_t>(16, lua_rawlen(state, -1)));
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(state, -1, i);
            if (lua_istable(state, -1)) {
                context.menu.emplace_back();
                auto &item = context.menu.back();
                for (auto field : {std::pair{"label", &item.label}, {"help", &item.help}, {"action", &item.action}}) {
                    rawField(state, -1, field.first);
                    if (lua_type(state, -1) == LUA_TSTRING) {
                        size_t length = 0;
                        const char *text = lua_tolstring(state, -1, &length);
                        field.second->assign(text, length);
                    }
                    lua_pop(state, 1);
                }
                if (item.label.empty() || item.action.empty())
                    context.menu.pop_back();
            }
            lua_pop(state, 1);
        }
    }
    return 0;
}
bool run(Invocation &context) {
    auto &m = context.module;
    m.exhausted = false;
    lua_sethook(m.state, budget, LUA_MASKCOUNT, kInstructionLimit);
    // Light C functions/userdata do not allocate, and the empty Lua stack has spare slots.
    lua_pushcfunction(m.state, invoke);
    lua_pushlightuserdata(m.state, &context);
    int status = lua_pcall(m.state, 1, 0, 0);
    lua_sethook(m.state, nullptr, 0, 0);
    if (status == LUA_OK && !m.exhausted)
        return true;
    const char *message =
        status != LUA_OK && lua_type(m.state, -1) == LUA_TSTRING ? lua_tostring(m.state, -1) : "non-string Lua error";
    log::warning("mod {} disabled: {}", m.name, m.exhausted ? "instruction budget exhausted" : message);
    m.failed = true;
    lua_settop(m.state, 0);
    return false;
}
} // namespace
void load(const std::filesystem::path &directory) {
    std::lock_guard lock(mutex);
    modules.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
        return;
    std::vector<std::filesystem::path> files;
    for (const auto &item : std::filesystem::directory_iterator(directory)) {
        auto path = item.path();
        if (item.is_directory()) {
            if (std::filesystem::exists(path / "disabled"))
                continue;
            path /= "main.lua";
        }
        if (path.extension() == ".lua" && std::filesystem::is_regular_file(path))
            files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    for (const auto &file : files) {
        if (modules.size() >= 128 || std::filesystem::file_size(file) > 1024 * 1024)
            continue;
        auto m = std::make_unique<Module>();
        m->name = file.parent_path().filename().string() + "/" + file.filename().string();
        m->state = lua_newstate(allocate, m.get());
        if (!m->state)
            continue;
        *static_cast<Module **>(lua_getextraspace(m->state)) = m.get();
        Invocation context{*m, Operation::Load, file.c_str()};
        if (!run(context))
            continue;
        log::info("runtime mod loaded: {}", m->name);
        modules.push_back(std::move(m));
    }
}
Value call(const char *hook, const std::vector<Value> &arguments, Value fallback) {
    std::lock_guard lock(mutex);
    for (auto &m : modules) {
        if (m->failed)
            continue;
        Invocation context{*m, Operation::Call, hook, &arguments};
        if (run(context) && context.result)
            fallback = std::move(*context.result);
    }
    return fallback;
}
std::vector<MenuItem> imageMenu(bool portrait, bool logo, bool onboarding) {
    std::lock_guard lock(mutex);
    const std::vector<Value> arguments{portrait, logo, onboarding};
    std::vector<MenuItem> out;
    for (auto &m : modules) {
        if (m->failed)
            continue;
        Invocation context{*m, Operation::Menu, "image_menu", &arguments};
        if (run(context))
            for (auto &item : context.menu)
                out.push_back(std::move(item));
    }
    return out;
}
bool feature(const char *name) {
    auto v = call("feature", {std::string(name)}, false);
    auto *p = std::get_if<bool>(&v);
    return p && *p;
}
std::string text(const char *hook, const std::vector<Value> &args, std::string fallback) {
    auto v = call(hook, args, fallback);
    auto *p = std::get_if<std::string>(&v);
    return p ? *p : fallback;
}
double number(const char *hook, const std::vector<Value> &args, double fallback) {
    auto v = call(hook, args, fallback);
    auto *p = std::get_if<double>(&v);
    return p ? *p : fallback;
}
std::vector<std::string> loaded() {
    std::lock_guard lock(mutex);
    std::vector<std::string> out;
    for (auto &m : modules)
        if (!m->failed)
            out.push_back(m->name);
    return out;
}
} // namespace oj::mods::runtime
