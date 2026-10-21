#pragma once
#include "Core/Data/Controls.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <span>

namespace oj::mac {
struct BindableKey {
    const char *semantic;
    const char *label;
    SDL_Scancode scan;
};
struct BindingAction {
    const char *id;
    const char *label;
    const char *group;
    const char *action;
    bool second;
    const char *defaultKey;
};
using Bindings = std::vector<std::string>;
std::span<const BindableKey> bindableKeys();
std::span<const BindingAction> bindingActions();
const BindableKey *bindingKey(std::string_view semantic);
uint8_t gameScanCode(SDL_Scancode scan);
Bindings defaultBindings();
bool validBindings(const Bindings &values);
std::string rebind(Bindings &values, size_t action, std::string_view key);
Bindings loadBindings(const std::filesystem::path &path);
bool saveBindings(const std::filesystem::path &path, const Bindings &values, std::string &error);
std::filesystem::path bindingsPath();
void applyBindings(std::vector<ActionMap> &maps, const Bindings &values);
void requestControlsEditor();
bool takeControlsRequest();
} // namespace oj::mac
