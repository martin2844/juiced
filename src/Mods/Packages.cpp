#include "Mods/Packages.h"
#include "Core/Base/GameFiles.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace oj::mods::packages {
namespace {
std::filesystem::path directory;
std::vector<Package> packages;
std::string read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), {}};
}
void write(const std::filesystem::path &path, const std::string &data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(data.data(), data.size());
    file.close();
    if (!file)
        throw std::runtime_error("Cannot write " + path.string());
}
bool safePath(const std::filesystem::path &path) {
    if (path.empty() || path.is_absolute())
        return false;
    for (const auto &part : path)
        if (part == ".." || part == ".")
            return false;
    return true;
}
std::string hash(const std::string &data) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : data)
        h = (h ^ c) * 1099511628211ull;
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << h;
    return out.str();
}
std::string unhex(const std::string &text) {
    if (text.size() % 2)
        throw std::runtime_error("Invalid patch text");
    const auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        throw std::runtime_error("Invalid patch hex digit");
    };
    std::string out;
    for (size_t i = 0; i < text.size(); i += 2)
        out += char(digit(text[i]) * 16 + digit(text[i + 1]));
    return out;
}
std::string retail(const GameFiles &game, const std::filesystem::path &name) {
    if (!safePath(name))
        throw std::runtime_error("Invalid patch source path");
    const auto data = game.read(name);
    if (!data)
        throw std::runtime_error("Missing game data: " + name.string());
    return {reinterpret_cast<const char *>(data->data()), data->size()};
}
void patch(const GameFiles &game, const std::filesystem::path &file, const std::filesystem::path &target,
           const std::filesystem::path &relative) {
    std::istringstream recipe(read(file));
    std::string op, source, expected, base, result;
    recipe >> op;
    if (op == "base") {
        recipe >> source >> expected;
        base = retail(game, source);
        if (hash(base) != expected)
            throw std::runtime_error("Game data differs from package recipe: " + source);
        if (std::filesystem::exists(target))
            throw std::runtime_error("Conflicting package patch: " + relative.string());
    } else if (op == "append") {
        result = std::filesystem::exists(target) ? read(target) : retail(game, relative);
    } else
        throw std::runtime_error("Invalid package recipe: " + file.string());
    while (recipe >> op) {
        if (op == "copy") {
            size_t start, size;
            if (!(recipe >> start >> size) || start > base.size() || size > base.size() - start)
                throw std::runtime_error("Invalid patch copy range");
            result.append(base, start, size);
        } else if (op == "text") {
            std::string text;
            if (!(recipe >> text))
                throw std::runtime_error("Missing patch text");
            result += unhex(text);
        } else
            throw std::runtime_error("Unknown patch operation: " + op);
        if (result.size() > 32 * 1024 * 1024)
            throw std::runtime_error("Package script exceeds size limit");
    }
    // Never truncate a hardlinked source asset.
    if (std::filesystem::exists(target))
        std::filesystem::remove(target);
    write(target, result);
}
} // namespace
void seedMissing(const std::filesystem::path &defaults, const std::filesystem::path &root) {
    if (!std::filesystem::is_directory(defaults))
        throw std::runtime_error("Missing bundled mod packages: " + defaults.string());
    std::filesystem::create_directories(root);
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for (auto it = std::filesystem::recursive_directory_iterator(defaults); it != std::filesystem::recursive_directory_iterator(); ++it) {
        const auto& file = *it;
        const auto name = file.path().filename();
        if (file.is_directory() && (name == ".cache" || name == "tools" || name == ".git")) {
            it.disable_recursion_pending();
            continue;
        }
        if (name == ".gitignore" || name == "disabled" || name == "disabled.tmp") continue;
        if (!file.is_regular_file())
            continue;
        const auto target = root / file.path().lexically_relative(defaults);
        if (std::filesystem::exists(target))
            continue;
        std::filesystem::create_directories(target.parent_path());
        auto temporary = target;
        temporary += ".seed-" + unique;
        try {
            std::filesystem::copy_file(file.path(), temporary);
            if (std::filesystem::exists(target))
                std::filesystem::remove(temporary);
            else
                std::filesystem::rename(temporary, target);
        } catch (...) {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
            throw;
        }
    }
}
void discover(const std::filesystem::path &root) {
    directory = root;
    packages.clear();
    if (!std::filesystem::is_directory(root))
        return;
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        const auto id = entry.path().filename().string();
        if (!entry.is_directory() || id.empty() || id[0] == '.' ||
            id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos)
            continue;
        std::ifstream manifest(entry.path() / "mod.ini");
        if (!manifest)
            continue;
        Package p;
        p.id = id;
        p.name = id;
        p.folder = entry.path();
        std::string line, kind;
        while (std::getline(manifest, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto equal = line.find('=');
            const auto key = line.substr(0, equal), value = equal == std::string::npos ? "" : line.substr(equal + 1);
            if (key == "name")
                p.name = value;
            if (key == "version")
                p.version = value;
            if (key == "kind")
                kind = value;
            if (key == "catalogue")
                p.catalogue = value == "true";
        }
        if (p.version != "1" || (kind != "script" && kind != "assets"))
            throw std::runtime_error("Unsupported package manifest: " + id);
        p.assets = kind == "assets";
        p.catalogue = p.catalogue || p.assets;
        if (!p.assets && !std::filesystem::is_regular_file(p.folder / "main.lua"))
            throw std::runtime_error("Missing package script: " + id);
        p.enabled = p.active = !std::filesystem::exists(p.folder / "disabled");
        packages.push_back(std::move(p));
    }
    std::sort(packages.begin(), packages.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
}
const std::vector<Package> &list() {
    return packages;
}
bool available() {
    return !packages.empty();
}
bool active(const std::string &id) {
    for (const auto &p : packages)
        if (p.id == id)
            return p.active;
    return false;
}
std::string toggle(const std::string &id) {
    for (auto &p : packages)
        if (p.id == id) {
            try {
                const auto marker = p.folder / "disabled";
                if (p.enabled) {
                    const auto temp = p.folder / "disabled.tmp";
                    write(temp, "Disabled from the in-game Mods menu.\n");
                    std::filesystem::rename(temp, marker);
                } else if (!std::filesystem::remove(marker)) {
                    throw std::runtime_error("Disabled marker is missing; restart to refresh the mod list");
                }
                p.enabled = !p.enabled;
                return {};
            } catch (const std::exception &e) {
                return e.what();
            }
        }
    return "Mod was not found.";
}
std::filesystem::path assemble(const std::filesystem::path &gameRoot) {
    if (!std::any_of(packages.begin(), packages.end(), [](const auto &p) { return p.active && p.assets; }))
        return {};
    const auto cache = directory / ".cache";
    const auto stage = cache / ("stage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(stage);
    try {
        const GameFiles game(gameRoot);
        for (const auto &p : packages) {
            if (!p.active || !p.assets)
                continue;
            const auto assets = p.folder / "assets";
            if (std::filesystem::is_directory(assets)) {
                for (const auto &file : std::filesystem::recursive_directory_iterator(assets)) {
                    if (!file.is_regular_file())
                        continue;
                    const auto target = stage / file.path().lexically_relative(assets);
                    std::filesystem::create_directories(target.parent_path());
                    if (std::filesystem::exists(target)) {
                        if (read(target) != read(file.path()))
                            throw std::runtime_error("Conflicting mod asset: " + target.filename().string());
                        continue;
                    }
                    std::error_code ec;
                    std::filesystem::create_hard_link(file.path(), target, ec);
                    if (ec)
                        std::filesystem::copy_file(file.path(), target);
                }
            }
            const auto patches = p.folder / "patches";
            if (std::filesystem::is_directory(patches)) {
                std::vector<std::filesystem::path> files;
                for (const auto &file : std::filesystem::recursive_directory_iterator(patches))
                    if (file.is_regular_file() && file.path().extension() == ".patch")
                        files.push_back(file.path());
                std::sort(files.begin(), files.end());
                for (const auto &file : files) {
                    auto relative = file.lexically_relative(patches);
                    relative.replace_extension();
                    if (!safePath(relative))
                        throw std::runtime_error("Invalid patch target");
                    patch(game, file, stage / relative, relative);
                }
            }
        }
        // A unique immutable overlay also allows multiple game sessions to coexist.
        return stage;
    } catch (...) {
        std::filesystem::remove_all(stage);
        throw;
    }
}
std::filesystem::path saveDirectory(const std::filesystem::path &base) {
    return base;
}
std::filesystem::path sharedSaveDirectory(const std::filesystem::path &support) {
    namespace fs = std::filesystem;
    const auto base = support / "ModSaves";
    fs::create_directories(base);
    std::vector<fs::path> roots{support / "saves", support / "LegacyModSaves"};
    if (fs::is_directory(base / "variants")) {
        std::vector<fs::path> variants;
        for (const auto &entry : fs::directory_iterator(base / "variants"))
            if (!entry.is_symlink() && entry.is_directory()) variants.push_back(entry.path());
        std::sort(variants.begin(), variants.end());
        roots.insert(roots.end(), variants.begin(), variants.end());
    }
    for (const auto &root : roots) {
        if (!fs::is_directory(root) || fs::is_symlink(root)) continue;
        if (fs::is_directory(root / "SAVE")) {
            for (const auto &entry : fs::directory_iterator(root / "SAVE")) {
                const auto name = entry.path().filename().string();
                if (entry.is_symlink() || !entry.is_directory() || !name.starts_with("P") ||
                    !fs::is_regular_file(entry.path() / "USERDATA")) continue;
                const auto key = hash(entry.path().lexically_relative(support).generic_string());
                const auto receipt = base / ".legacy-imports" / key;
                if (fs::exists(receipt)) continue;
                const auto plan = base / ".legacy-imports" / (key + ".target");
                auto destName = name;
                if (fs::exists(plan)) {
                    destName = read(plan);
                    if (destName.empty() || fs::path(destName).filename() != fs::path(destName) ||
                        !destName.starts_with("P")) throw std::runtime_error("Invalid legacy import target");
                } else {
                    if (fs::exists(base / "SAVE" / destName)) {
                        // Keep both careers on collisions; never overwrite the current profile.
                        destName += "-import-" + key;
                        unsigned suffix = 0;
                        while (fs::exists(base / "SAVE" / destName))
                            destName = name + "-import-" + key + "-" + std::to_string(++suffix);
                    }
                    write(plan, destName);
                }
                const auto dest = base / "SAVE" / destName;
                if (!fs::exists(dest)) {
                    const auto stage = base / (".import-" + key);
                    fs::remove_all(stage); // Only our interrupted, unpublished staging copy.
                    fs::copy(entry.path(), stage, fs::copy_options::recursive | fs::copy_options::skip_symlinks);
                    fs::create_directories(dest.parent_path());
                    fs::rename(stage, dest);
                }
                write(receipt, dest.filename().string());
            }
        }
        // Preserve per-profile creative settings and records without replacing live data.
        for (const auto &part : {fs::path("CreativeMods"), fs::path("SAVE/HS"), fs::path("reg.cf1")}) {
            const auto from = root / part, to = base / part;
            if (!fs::exists(from) || fs::is_symlink(from)) continue;
            fs::create_directories(to.parent_path());
            fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::skip_existing |
                               fs::copy_options::skip_symlinks);
        }
    }
    return base;
}
} // namespace oj::mods::packages
