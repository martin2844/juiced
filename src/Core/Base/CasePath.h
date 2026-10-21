#pragma once

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace oj {
// Retail data was authored for Windows. Android's filesystem distinguishes case,
// so resolve existing path components without changing names of newly created files.
inline std::filesystem::path gameNativePath(const std::filesystem::path &input) {
    auto text = input.string();
    std::replace(text.begin(), text.end(), '\\', '/');
    std::filesystem::path path(text);
#ifdef OJ_ANDROID
    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        return path;
    auto lower = [](std::string s) {
        for (auto &c : s)
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        return s;
    };
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::map<std::string, std::filesystem::path>> directories;
    std::lock_guard lock(mutex);
    auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
    if (ec)
        return path;
    auto current = absolute.root_path();
    for (const auto &part : absolute.relative_path()) {
        auto exact = current / part;
        if (std::filesystem::exists(exact, ec)) {
            current = exact;
            continue;
        }
        auto found = directories.find(current);
        if (found == directories.end()) {
            std::map<std::string, std::filesystem::path> entries;
            for (const auto &entry : std::filesystem::directory_iterator(current, ec))
                entries.emplace(lower(entry.path().filename().string()), entry.path().filename());
            found = directories.emplace(current, std::move(entries)).first;
        }
        auto match = found->second.find(lower(part.string()));
        current /= match == found->second.end() ? part : match->second;
    }
    return current;
#else
    return path;
#endif
}
} // namespace oj
