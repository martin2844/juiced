#include "Core/Data/Language.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"

#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <set>

namespace oj {

namespace {

constexpr Language kLanguages[] = {
    {0, "english", "eng", 'e', "eng"}, {1, "french", "fre", 'f', "fra"}, {2, "german", "ger", 'g', "ger"},
    {3, "italian", "ita", 'i', "ita"}, {4, "spanish", "spa", 's', "spa"}, {5, "czech", "czk", 'c', "czk"},
    {6, "polish", "pol", 'p', "pol"},  {7, "russian", "rus", 'r', "rus"},
};

// The first of `names` the data has; a missing first logged once with the one in its place; the first when none is
// there (its reader says so).
std::filesystem::path firstPresent(const GameFiles& files, std::initializer_list<std::filesystem::path> names) {
    const std::filesystem::path& own = *names.begin();
    for (const std::filesystem::path& name : names) {
        if (!files.exists(name)) continue;
        if (name != own) {
            static std::mutex lock;
            static std::set<std::filesystem::path> told;
            const std::lock_guard<std::mutex> hold(lock);
            if (told.insert(own).second) log::warning("no {}: {} in its place", own.string(), name.string());
        }
        return name;
    }
    return own;
}

}  // namespace

std::optional<Language> Language::byIndex(int index) {
    for (const Language& l : kLanguages) {
        if (l.index == index) {
            return l;
        }
    }
    return std::nullopt;
}

std::optional<Language> Language::byName(std::string_view name) {
    for (const Language& l : kLanguages) {
        const std::string_view own = l.name;
        if (name.size() != own.size()) continue;
        bool same = true;
        for (size_t i = 0; i < name.size() && same; ++i) {
            same = std::tolower(static_cast<unsigned char>(name[i])) == static_cast<unsigned char>(own[i]);
        }
        if (same) return l;
    }
    return std::nullopt;
}

std::filesystem::path textTableFile(const GameFiles& files, const Language& language) {
    const std::filesystem::path scripts = "scripts";
    return firstPresent(files, {scripts / (std::string(language.name) + ".txt"), scripts / "english.txt"});
}

std::filesystem::path controlsFile(const GameFiles& files, const Language& language) {
    const std::filesystem::path scripts = "scripts";
    return firstPresent(files, {scripts / (std::string("controls_dx9_") + language.controlsLetter + ".txt"),
                                scripts / "controls_dx9_e.txt", scripts / "controls_dx9.txt"});
}

std::filesystem::path jetTextFile(const GameFiles& files, const Language& language) {
    const std::filesystem::path scripts = "scripts";
    return firstPresent(files, {scripts / ("jettext" + std::string(1, language.controlsLetter) + ".txt"),
                                scripts / "jettexte.txt"});
}

std::string speechBank(const GameFiles& files, const std::string& kind, char letter) {
    const std::string own = kind + letter, english = kind + 'e';
    const std::filesystem::path speech = std::filesystem::path("audio") / "speech";
    const std::filesystem::path chosen =
        firstPresent(files, {speech / own / (own + ".dsb"), speech / english / (english + ".dsb")});
    return chosen.parent_path().filename().string();
}

int languageOfLangId(uint32_t langId) {
    switch (langId & 0x3FF) {
    case 0x07: return 2;
    case 0x0A: return 4;
    case 0x0C: return 1;
    case 0x10: return 3;
    case 0x05: return 5;
    case 0x15: return 6;
    case 0x19: return 7;
    default: return 0;
    }
}

bool TextTable::load(const std::filesystem::path& file) {
    m_lines.clear();
    // A path of the game's (its archives as the game mounts them).
    const auto data = GameFiles(std::filesystem::current_path()).read(file);
    if (!data) {
        return false;
    }
    const std::string bytes(reinterpret_cast<const char*>(data->data()), data->size());
    std::wstring text;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        text.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) |
                                            (static_cast<unsigned char>(bytes[i + 1]) << 8)));
    }
    text.push_back(L'\0');
    const auto at = [&](size_t i) { return i < text.size() ? text[i] : L'\0'; };
    size_t p = text.size() > 1 ? 1 : 0;
    if (text[p] == L' ') ++p;
    while (p + 1 < text.size()) {
        const wchar_t c = text[p];
        if ((c == L'E' && at(p + 1) == L'N' && at(p + 2) == L'D' && at(p + 3) != L'E') ||
            (c == L' ' && ((at(p + 1) == L'E' && at(p + 2) == L'N' && at(p + 3) == L'D') ||
                           (at(p + 1) == L'e' && at(p + 2) == L'n' && at(p + 3) == L'd'))) ||
            (c == L'e' && at(p + 1) == L'n' && at(p + 2) == L'd')) {
            break;
        }
        std::wstring line;
        bool comment = false;
        while (p + 1 < text.size()) {
            while (text[p] == L'"') ++p;
            const wchar_t d = text[p];
            if (d == L'#') {
                comment = true;
                ++p;
                while (p + 1 < text.size() && text[p] != L'\r' && at(p + 1) != L'\n') ++p;
                p += 2;
                break;
            }
            if (d == L'\r' && at(p + 1) == L'\n') {
                p += 2;
                break;
            }
            if (d == L'\r' && at(p + 1) == L'\r' && at(p + 2) == L'\n') {
                p += 3;
                break;
            }
            if (d == L'\0') break;
            if (d == L'\\' && at(p + 1) == L'n') {
                line.push_back(L'\n');
                p += 2;
            } else {
                line.push_back(d);
                ++p;
            }
        }
        if (comment) continue;
        static const wchar_t* const kIllegal[][2] = {
            {L"ILLEGAL", L"PINKSLIP"}, {L"Illegal", L"PinkSlip"}, {L"illegal", L"pinkslip"}};
        for (const auto& pair : kIllegal) {
            if (line == pair[0]) {
                line = pair[1];
                break;
            }
        }
        m_lines.push_back(std::move(line));
    }
    return true;
}

void TextTable::set(size_t index, std::wstring text) {
    if (index < m_lines.size()) m_lines[index] = std::move(text);
}

const std::wstring& TextTable::at(size_t index) const {
    static const std::wstring empty;
    return index < m_lines.size() ? m_lines[index] : empty;
}

}  // namespace oj
