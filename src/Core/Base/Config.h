#pragma once
// Settings of OpenJuice itself from OpenJuice.ini next to the executable (the game's own settings are those of
// JuicedConfig.exe in the registry). A missing file is written with the defaults so that it can be edited.

#include <filesystem>
#include <string>

namespace oj {

struct Config {
    std::string language;  // [Game] Language: english .. russian; empty: by the language id, as 1.0.1
    // [Game] SkipIntroMovies: 0 (the default) plays the opening movies as the stock game does; 1 passes their windows
    // (splash, juicesplash, skippable) at once.
    bool skipIntroMovies = false;
    // [Saves] Folder, the debug builds only: where SAVE is; empty (always in the release builds): the stock game's
    // (APPDATA THQ Juiced).
    std::filesystem::path saveFolder;

    static Config load(const std::filesystem::path& file);
    void save(const std::filesystem::path& file) const;
};

}  // namespace oj
