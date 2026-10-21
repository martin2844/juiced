#include "Core/Base/GameFiles.h"
#include "Core/Base/CasePath.h"

#include "Core/Formats/DatArchive.h"

#include <fstream>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

namespace oj {

namespace {

// The archives looked for, by their path (none where there is none), read once for every GameFiles.
std::mutex g_archivesLock;
std::map<std::wstring, std::shared_ptr<const DatArchive>> g_archives;

std::shared_ptr<const DatArchive> archiveAt(const std::filesystem::path& file) {
    std::wstring key = file.wstring();
    for (wchar_t& c : key) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        if (c == L'/') c = L'\\';
    }
    const std::lock_guard<std::mutex> lock(g_archivesLock);
    if (const auto it = g_archives.find(key); it != g_archives.end()) return it->second;
    std::shared_ptr<const DatArchive> archive;
    std::error_code ec;
    if (std::filesystem::is_regular_file(gameNativePath(file), ec)) {
        if (auto opened = DatArchive::open(gameNativePath(file))) archive = std::make_shared<const DatArchive>(std::move(*opened));
    }
    g_archives[key] = archive;
    return archive;
}

// Where a path of the game's lies: its loose file and, under an archive's folder, the archive and the name inside.
struct Located {
    std::filesystem::path loose;
    std::shared_ptr<const DatArchive> archive;
    std::string inside;
};

Located locate(const std::filesystem::path& root, const std::filesystem::path& relative) {
    Located l;
    std::filesystem::path base = root;
    std::string normalized = relative.string();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::filesystem::path inGame = normalized;
    if (relative.has_root_path()) {
        // A full path: inside the game's folder from there, else a file of its own.
        inGame = relative.lexically_relative(root);
        if (inGame.empty() || *inGame.begin() == "..") {
            l.loose = relative;
            return l;
        }
    }
    std::vector<std::filesystem::path> parts;
    for (const std::filesystem::path& part : inGame) {
        if (!part.empty() && part != "." && part != "\\" && part != "/") parts.push_back(part);
    }
    if (parts.empty()) {
        l.loose = root / relative;
        return l;
    }
    // Each folder down the path: with an archive mounted the rest is a path inside it; else a <folder>.dat beside
    // the folder mounts it. The loose file is the same path from the root either way.
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (l.archive) {
            l.inside += parts[i].string() + "\\";
        } else {
            std::filesystem::path dat = base / parts[i];
            dat += L".dat";
            l.archive = archiveAt(dat);
        }
        base /= parts[i];
    }
    if (l.archive) l.inside += parts.back().string();
    l.loose = base / parts.back();
    return l;
}

std::optional<std::vector<std::byte>> readLoose(const std::filesystem::path& file) {
    std::ifstream in(gameNativePath(file), std::ios::binary | std::ios::ate);
    if (!in) {
        return std::nullopt;
    }
    const std::streamsize size = in.tellg();
    std::vector<std::byte> data(static_cast<size_t>(size));
    in.seekg(0);
    if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size)) {
        return std::nullopt;
    }
    return data;
}

}  // namespace

GameFiles::GameFiles(std::filesystem::path root) : m_root(std::move(root)) {}

bool GameFiles::valid() const {
    std::error_code ec;
    return !m_root.empty() && std::filesystem::is_directory(gameNativePath(m_root / L"tracks"), ec);
}

std::filesystem::path GameFiles::resolve(const std::filesystem::path& relative) const {
    return gameNativePath(m_root / relative);
}

namespace {

char g_languageLetter = 0;

// 0x004F9DB3: the name with "_<letter>" before its extension; none without a language, an extension, or with a '_'
// two characters before the '.'.
std::optional<std::filesystem::path> localized(const std::filesystem::path& relative) {
    if (g_languageLetter == 0) return std::nullopt;
    const std::wstring text = relative.wstring();
    const size_t dot = text.rfind(L'.');
    if (dot == std::wstring::npos) return std::nullopt;
    if (dot >= 2 && text[dot - 2] == L'_') return std::nullopt;
    std::wstring named = text.substr(0, dot);
    named += L'_';
    named += static_cast<wchar_t>(g_languageLetter);
    named += text.substr(dot);
    return std::filesystem::path(named);
}

bool existsAt(const std::filesystem::path& root, const std::filesystem::path& relative) {
    const Located l = locate(root, relative);
    if (l.archive) return l.archive->contains(l.inside);
    std::error_code ec;
    return std::filesystem::is_regular_file(gameNativePath(l.loose), ec);
}

std::optional<std::vector<std::byte>> readAt(const std::filesystem::path& root, const std::filesystem::path& relative) {
    const Located l = locate(root, relative);
    if (l.archive) {
        if (auto data = l.archive->read(l.inside)) return data;
    }
    return readLoose(l.loose);
}

}  // namespace

void GameFiles::setLanguageLetter(char letter) {
    g_languageLetter = letter;
}

bool GameFiles::exists(const std::filesystem::path& relative) const {
    if (existsAt(m_root, relative)) return true;
    const auto other = localized(relative);
    return other && existsAt(m_root, *other);
}

std::optional<std::vector<std::byte>> GameFiles::read(const std::filesystem::path& relative) const {
    if (auto data = readAt(m_root, relative)) return data;
    if (const auto other = localized(relative)) return readAt(m_root, *other);
    return std::nullopt;
}

}  // namespace oj
