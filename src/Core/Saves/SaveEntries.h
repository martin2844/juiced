#pragma once
// The smaller entries of juicey.dat (docs/spec/03-saves.md): UIHelp.sav, ArcadeUnlock.sav, settings.dat.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oj::save {

// UIHelp.sav (0x00485210): a u32 per help page of the table at 0x00715D48, set once the page was shown.
struct UiHelp {
    static constexpr const char* kPages[] = {
        "CMSCalendar", "CMSCarMenu",    "CMSRespectOverview", "CMSCrew",        "CMSMobilePhone",
        "CMSGridwalk", "CMSRaceSetup",  "CMSCrewRaceSetup",   "CMSCalendar",    "CMSAtMeeting",
        "instant_selcar", "tuningmenu", "stylingmenu",
    };
    std::vector<uint32_t> shown;

    bool read(const std::vector<uint8_t>& data);
    std::vector<uint8_t> write() const;
};

// ArcadeUnlock.sav (0x0042ABF0): per arcade series 5 races of two u32 (the series object's +0xCC / +0xD0 per race,
// 0xD4 apart), then 0xD4 and 0x1B4 bytes of the arcade object (+0x52F0, +0x513C) and its u32 +0x5138 and +0x5134.
// The series count is the game's (15 in the release), not stored.
struct ArcadeUnlock {
    static constexpr size_t kCarFlagsSize = 53 * 4;     // 0xD4
    static constexpr size_t kRouteFlagsSize = 109 * 4;  // 0x1B4
    struct Race {
        uint32_t locked = 0;     // +0xCC
        uint32_t completed = 0;  // +0xD0
    };
    std::vector<std::array<Race, 5>> series;
    std::vector<uint8_t> carFlags;    // the arcade's +0x52F0: by car id, 0 = open
    std::vector<uint8_t> routeFlags;  // +0x513C: by route, 0 = open
    uint32_t developersOpen = 0;  // +0x5138: the developers' championships open
    uint32_t extremeOpen = 0;     // +0x5134: the extreme nitrous championships open

    bool read(const std::vector<uint8_t>& data);
    std::vector<uint8_t> write() const;
};

// settings.dat (0x00409A30): fields of the settings object (+0x54 of the save system object) in the order written.
// The object is kept whole; the bytes it does not save stay 0.
struct Settings {
    static constexpr size_t kObjectSize = 0x354;

    // The object's fields by their offsets in it (the save system's are 0x54 more). "Per player": the second player's
    // value is 4 further.
    enum Field : uint32_t {
        kMusicVolume = 0x00,       // float
        kEffectsVolume = 0x04,     // float
        kSpeechVolume = 0x08,      // float
        kSpeakers = 0x0C,
        kCustomDifficulty = 0x10,  // float: the custom race's
        kCustomLaps = 0x14,
        kCustomOpponents = 0x18,
        kLogo = 0x20,              // the crew logo (one of ui_chooselogo_01 ..)
        kSteeringWheel = 0x24,     // a steering wheel used (wheeloptions; gameoptions' VIB flips the same value)
        kAids = 0x28,              // the driving aids shown
        kMap = 0x2C,               // the race's map shown
        kAutomatic = 0x30,         // per player: the automatic gearbox
        kView = 0x38,              // per player: the race camera's view
        kMph = 0x40,               // the units: miles an hour (1 for English)
        kOnline = 0x44,
        kVoiceChat = 0x48,
        kDifficulty = 0x4C,        // the AI's
        kWheelStrength = 0x50,     // per player, float: the force feedback's strength
        kWheelSpeed = 0x58,        // per player: not speed sensitive steering
        kWheelForce = 0x60,        // per player: the force feedback on
        kSensitivity = 0x70,       // per player, float: the steering's
        kDriverMap = 0x78,         // per device: the DRIVER map chosen (DRIVER1 .. DRIVER4)
        kCustomTimeOfDay = 0x88,
        kCustomWeather = 0x8C,
        kCrewName = 0x90,          // 8-bit characters
        kScheme = 0xA0,            // the crew's paint scheme (17 values: base, metallic, pearl, the amounts, ..)
        kSongs = 0x208,            // a bit per song played
        kSongsKnown = 0x218,       // the songs the bits were set for
        kVoiceInput = 0x2D4,       // the capture device's name (0x40 bytes)
        kVoiceOutput = 0x314,      // the playback device's
    };
    std::array<uint8_t, kObjectSize> object{};

    // The fields in the stock order (offset, size): 781 bytes in all.
    static const std::vector<std::pair<uint32_t, uint32_t>>& layout();

    // The values of a new driver (the constructor 0x00409750, then 0x004097F0); languageIndex: the game's language
    // (+0x40 is 1 for English only).
    static Settings defaults(int languageIndex);
    // 0x004097F0: a new driver's values over these (the title's enter, 0x00467060: going back to the start forgets
    // the driver's volumes and the rest); the scheme's first five values (the constructor's) and the bytes it does
    // not write stay as they are.
    void reset(int languageIndex);

    uint32_t u32(uint32_t offset) const;
    void setU32(uint32_t offset, uint32_t value);
    // +0x90: the crew name, 8-bit characters (the low byte of each character typed), ending with 0.
    std::string crewName() const;
    void setCrewName(std::wstring_view name);
    // +0x20: the crew logo (one of ui_chooselogo_01 ..).
    uint32_t logo() const { return u32(kLogo); }
    // UTF-16 texts ending with 0: the GameSpy account at +0xE4 (the nick, 31 characters saved), +0x122 (the email,
    // 51) and +0x188 (the password, 31, plain), the nick of the online setups at +0x220 and +0x26C. The text up to its
    // 0 or `chars`; written with its 0, at most `chars` with it.
    static constexpr uint32_t kOnlineNick = 0xE4;
    static constexpr uint32_t kOnlineEmail = 0x122;
    static constexpr uint32_t kOnlinePassword = 0x188;
    std::wstring wideText(uint32_t offset, size_t chars) const;
    void setWideText(uint32_t offset, std::wstring_view text, size_t chars);

    bool read(const std::vector<uint8_t>& data);
    std::vector<uint8_t> write() const;
};

}  // namespace oj::save
