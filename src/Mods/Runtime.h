#pragma once
#include <filesystem>
#include <string>
#include <variant>
#include <vector>
namespace oj::mods::runtime {
using Value = std::variant<double, std::string, bool>;
// Each mod returns an API-v1 table of callbacks. Missing/erroring hooks leave the host value unchanged.
struct MenuItem {
    std::string label, help, action;
};
std::vector<MenuItem> imageMenu(bool portrait, bool logo, bool onboarding);
void load(const std::filesystem::path &directory);
Value call(const char *hook, const std::vector<Value> &arguments, Value fallback);
bool feature(const char *name);
std::string text(const char *hook, const std::vector<Value> &arguments, std::string fallback = {});
double number(const char *hook, const std::vector<Value> &arguments, double fallback);
std::vector<std::string> loaded();
} // namespace oj::mods::runtime
