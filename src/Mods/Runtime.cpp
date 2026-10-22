#include "Mods/Runtime.h"
#include "Core/Base/Log.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <lua.hpp>
#include <memory>
#include <mutex>
namespace oj::mods::runtime {
namespace {
struct Module {
    std::string name;
    size_t bytes = 0;
    lua_State *state = nullptr;
    bool failed = false;
    ~Module() {
        if (state)
            lua_close(state);
    }
};
std::vector<std::unique_ptr<Module>> modules;
std::recursive_mutex mutex;
void *allocate(void *context, void *ptr, size_t oldSize, size_t size) {
    auto &m = *static_cast<Module *>(context);
    if (!ptr)
        oldSize = 0;
    if (!size) {
        m.bytes -= oldSize;
        free(ptr);
        return nullptr;
    }
    if (size > 16 * 1024 * 1024 || m.bytes - oldSize > 16 * 1024 * 1024 - size)
        return nullptr;
    auto *next = realloc(ptr, size);
    if (next)
        m.bytes = m.bytes - oldSize + size;
    return next;
}
void budget(lua_State *state, lua_Debug *) {
    luaL_error(state, "mod callback exceeded instruction budget");
}
void push(lua_State *state, const Value &v) {
    if (auto *n = std::get_if<double>(&v))
        lua_pushnumber(state, *n);
    else if (auto *s = std::get_if<std::string>(&v))
        lua_pushlstring(state, s->data(), s->size());
    else
        lua_pushboolean(state, std::get<bool>(v));
}
bool run(Module &m, int argc) {
    lua_sethook(m.state, budget, LUA_MASKCOUNT, 100000);
    int status = lua_pcall(m.state, argc, 1, 0);
    lua_sethook(m.state, nullptr, 0, 0);
    if (status == LUA_OK)
        return true;
    log::warning("mod {} disabled: {}", m.name, lua_tostring(m.state, -1));
    m.failed = true;
    lua_pop(m.state, 1);
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
        // No filesystem, shell, package loading, bytecode loading or debugging APIs.
        for (auto lib : {std::pair<const char *, lua_CFunction>{"_G", luaopen_base},
                         {"math", luaopen_math},
                         {"string", luaopen_string},
                         {"table", luaopen_table},
                         {"utf8", luaopen_utf8}}) {
            luaL_requiref(m->state, lib.first, lib.second, 1);
            lua_pop(m->state, 1);
        }
        for (auto key : {"dofile", "loadfile", "load", "collectgarbage", "print"}) {
            lua_pushnil(m->state);
            lua_setglobal(m->state, key);
        }
        if (luaL_loadfilex(m->state, file.c_str(), "t") != LUA_OK) {
            log::warning("mod {} not loaded: {}", m->name, lua_tostring(m->state, -1));
            continue;
        }
        if (!run(*m, 0) || !lua_istable(m->state, -1))
            continue;
        lua_getfield(m->state, 1, "api");
        bool compatible = lua_tointeger(m->state, -1) == 1;
        lua_pop(m->state, 1);
        if (!compatible) {
            log::warning("mod {} needs unsupported API", m->name);
            continue;
        }
        log::info("runtime mod loaded: {}", m->name);
        modules.push_back(std::move(m));
    }
}
Value call(const char *hook, const std::vector<Value> &arguments, Value fallback) {
    std::lock_guard lock(mutex);
    for (auto &m : modules) {
        if (m->failed)
            continue;
        auto *l = m->state;
        lua_getfield(l, 1, hook);
        if (!lua_isfunction(l, -1)) {
            lua_pop(l, 1);
            continue;
        }
        for (const auto &arg : arguments)
            push(l, arg);
        if (!run(*m, int(arguments.size())))
            continue;
        switch (lua_type(l, -1)) {
        case LUA_TNUMBER:
            if (std::isfinite(lua_tonumber(l, -1)))
                fallback = double(lua_tonumber(l, -1));
            break;
        case LUA_TSTRING:
            fallback = std::string(lua_tostring(l, -1));
            break;
        case LUA_TBOOLEAN:
            fallback = bool(lua_toboolean(l, -1));
            break;
        }
        lua_pop(l, 1);
    }
    return fallback;
}
std::vector<MenuItem> imageMenu(bool portrait, bool logo, bool onboarding) {
    std::lock_guard lock(mutex);
    std::vector<MenuItem> out;
    for (auto &m : modules) {
        if (m->failed)
            continue;
        auto *l = m->state;
        lua_getfield(l, 1, "image_menu");
        if (!lua_isfunction(l, -1)) {
            lua_pop(l, 1);
            continue;
        }
        lua_pushboolean(l, portrait);
        lua_pushboolean(l, logo);
        lua_pushboolean(l, onboarding);
        if (!run(*m, 3))
            continue;
        if (lua_istable(l, -1))
            for (int i = 1; i <= int(std::min<size_t>(16, lua_rawlen(l, -1))); ++i) {
                lua_rawgeti(l, -1, i);
                if (lua_istable(l, -1)) {
                    MenuItem item;
                    for (auto field :
                         {std::pair{"label", &item.label}, {"help", &item.help}, {"action", &item.action}}) {
                        lua_getfield(l, -1, field.first);
                        if (lua_type(l, -1) == LUA_TSTRING)
                            *field.second = lua_tostring(l, -1);
                        lua_pop(l, 1);
                    }
                    if (!item.label.empty() && !item.action.empty())
                        out.push_back(std::move(item));
                }
                lua_pop(l, 1);
            }
        lua_pop(l, 1);
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
