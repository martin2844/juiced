#pragma once
// The game's languages: the global text table scripts\<name>.txt, the window texts ui\windows\<name>\<window>.<ext>,
// the controls file scripts\controls_dx9_<letter>.txt, the speech banks audio\speech\ui<letter> and race<letter>, the
// movies' sound track. The reference build was made for another region: its language is fixed to that region's.
// OpenJuice has the region binding removed and picks the language as the English build 1.0.1 does (languageOfLangId),
// with the three languages more; English when none is picked.

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

struct Language {
    int index = 0;
    const char* name = "english";   // english french german italian spanish czech polish russian
    const char* extension = "eng";  // eng fre ger ita spa czk pol rus
    char controlsLetter = 'e';      // e f g i s c p r
    // The window texts' extension of the windows read from the disk's folders (0x005AA660 with the window's +0x20,
    // which no window has set): French "fra", where its texts are .fre (so they are not found and the English ones
    // come up); the others as `extension`.
    const char* looseExtension = "eng";

    static std::optional<Language> byIndex(int index);
    static std::optional<Language> byName(std::string_view name);
};

// The language of the English build 1.0.1 (0x0059E300): the settings' `language` when it is not negative, else the
// user's language (GetUserDefaultLangID), taken as a Windows language id; its primary language: German (0x07) 2,
// Spanish (0x0A) 4, French (0x0C) 1, Italian (0x10) 3, any other English 0. Not of the stock: Czech (0x05) 5, Polish
// (0x15) 6 and Russian (0x19) 7, so all eight languages of the data are reached.
int languageOfLangId(uint32_t langId);

// A text file of the game as 0x005AE520 reads it: UTF-16, its first two bytes (the byte order mark, or a space where
// there is none) and a space after them passed over; one text per line (CR LF, or CR CR LF); the reading stops at a
// line starting "END" (not "ENDE"), " END", " end" or "end". Every double quote of a line is taken out wherever it
// stands, \n (the two characters) becomes a line break, a line holding # is a comment (no text); a text "ILLEGAL",
// "Illegal" or "illegal" becomes "PINKSLIP", "PinkSlip" or "pinkslip" (0x006C40E0). Index = text number from 0.
class TextTable {
public:
    bool load(const std::filesystem::path& file);
    const std::wstring& at(size_t index) const;
    // A line replaced (the stock game writes some, such as the player's crew's name); none past the table.
    void set(size_t index, std::wstring text);
    size_t size() const { return m_lines.size(); }

private:
    std::vector<std::wstring> m_lines;
};

}  // namespace oj
