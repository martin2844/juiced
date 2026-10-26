#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace oj::mods::packages {
struct Package {
    std::string id, name, version;
    std::filesystem::path folder;
    bool assets = false;
    bool catalogue = false;
    bool active = false;  // Immutable for the running session.
    bool enabled = false; // Desired state at next launch.
};
// Install missing bundled files without overwriting user edits or toggle markers.
void seedMissing(const std::filesystem::path &defaults, const std::filesystem::path &root);
// Discover manifests before loading scripts or any game data.
void discover(const std::filesystem::path &root);
const std::vector<Package> &list();
bool available();
bool active(const std::string &id);
std::string toggle(const std::string &id);
// Build an isolated overlay from enabled packages; throws on incompatible recipes.
std::filesystem::path assemble(const std::filesystem::path &game);
// Mod selection never changes the career directory.
std::filesystem::path saveDirectory(const std::filesystem::path &base);
// Recover legacy stock/variant careers by copying, never overwriting or deleting originals.
std::filesystem::path sharedSaveDirectory(const std::filesystem::path &support);
} // namespace oj::mods::packages
