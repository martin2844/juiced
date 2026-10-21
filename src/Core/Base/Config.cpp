#include "Core/Base/Config.h"

#include "Core/Base/Text.h"

#include <windows.h>

namespace oj {

namespace {

std::wstring readString(const wchar_t* section, const wchar_t* key, const std::wstring& fallback,
                        const std::filesystem::path& file) {
    wchar_t buffer[MAX_PATH * 2] = {};
    GetPrivateProfileStringW(section, key, fallback.c_str(), buffer, static_cast<DWORD>(std::size(buffer)),
                             file.c_str());
    return buffer;
}

}  // namespace

Config Config::load(const std::filesystem::path& file) {
    Config config;
    if (!std::filesystem::exists(file)) {
        config.save(file);
        return config;
    }
    config.language = toUtf8(readString(L"Game", L"Language", L"", file));
#ifdef _DEBUG
    config.saveFolder = readString(L"Saves", L"Folder", L"", file);
#endif
    config.skipIntroMovies = readString(L"Game", L"SkipIntroMovies", L"0", file) == L"1";
    return config;
}

void Config::save(const std::filesystem::path& file) const {
    WritePrivateProfileStringW(L"Game", L"Language", toWide(language).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Game", L"SkipIntroMovies", skipIntroMovies ? L"1" : L"0", file.c_str());
#ifdef _DEBUG
    WritePrivateProfileStringW(L"Saves", L"Folder", saveFolder.c_str(), file.c_str());
#endif
}

}  // namespace oj
