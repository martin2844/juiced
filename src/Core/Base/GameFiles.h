#pragma once
// Read access to the files of an installed Juiced. The engine ships no game data: models, textures, tracks and
// shaders are read from the user's own copy of the game, packed or not. As the game mounts its folders (0x004F9970):
// a folder with a <folder>.dat beside it is read from that archive (DatArchive), the folders under it from the
// archive's paths; a file the archive does not hold is read from the folder itself (0x004FCDA0), but is not seen to
// exist there (the archive's size, 0x004FD010, looks inside only).

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace oj {

class GameFiles {
public:
    explicit GameFiles(std::filesystem::path root);

    const std::filesystem::path& root() const { return m_root; }

    // A folder of the game: it holds tracks\ (OpenJuice needs no Juiced.exe).
    bool valid() const;

    // relative: a path inside the game folder, e.g. "tracks\Angel SuperSpeedway\Angel SuperSpeedway.tdf". The
    // loose file's path, archives aside (the audio and the movies are never packed).
    std::filesystem::path resolve(const std::filesystem::path& relative) const;
    // A name not found is looked for again localized (0x004F9DB3, with the language object [0x0075F79C]): "_" and the
    // language's letter (the table 0x00725694, "_e" .. "_r") before its last '.', unless the character two before
    // that is a '_' already; none without a '.'.
    bool exists(const std::filesystem::path& relative) const;
    std::optional<std::vector<std::byte>> read(const std::filesystem::path& relative) const;

    // The language's letter of the localized names (e f g i s c p r); 0 none (no language object yet).
    static void setLanguageLetter(char letter);

    // Optional loose-file mod overlay. Set before creating/loading game data; empty disables it.
    // Files absent from the overlay still come from the user's retail installation.
    static void setOverrideRoot(std::filesystem::path root);

private:
    std::filesystem::path m_root;
};

}  // namespace oj
