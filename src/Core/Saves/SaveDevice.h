#pragma once
// The save device of the stock game (docs/spec/03-saves.md, "The save device"): the windows that list, read, write
// and delete saves (driver, saveprofile, autosave, loadhighscores, savehighscores, ...) give it requests in a block
// of globals (0x00767FE8) and it answers there with state flags, one step a frame, a file at a time.

#include "Core/Data/JetTexts.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj::save {

// The state flags of the device (the request block's +0x1C + 4 k; [0x00768004 + 4 k]). The windows show the line k of
// the PC block of the language's jettext file (0x00728DA0 + 4 k, flagText) for most of them; a step clears them all.
// The PC's device never sets 1 .. 3 (their PC lines are empty); their names follow the PS2 block's lines.
enum class SaveFlag : int32_t {
    Checking = 0,      // "Checking hard disk..."
    Inserted = 1,      // (PS2: "Memory card is inserted")
    NoDevice = 2,      // (PS2: "No memory card")
    Unformatted = 3,   // (PS2: "Memory card is unformatted")
    NoSpace = 4,       // "Insufficient space on the hard disk!"
    NoData = 5,        // the list found nothing (+0x30)
    FormatAsk = 6,     // "Format the hard disk?"
    Exists = 7,        // the folder to write is there: overwrite? (+0x38)
    Loading = 8,       // "Loading data..."
    Saving = 9,        // the write starts (+0x40)
    Formatting = 10,   // (the PC's line empty)
    Loaded = 11,       // read (+0x48)
    Saved = 12,        // written (+0x4C)
    Formatted = 13,    // "Format Successful"
    Listed = 14,       // the list is made (+0x54)
    ReadError = 15,    // +0x58
    WriteError = 16,   // "Save failed! Data write error"
    FormatError = 17,  // "Format failed! Should NEVER see this msg"
    Corrupt = 18,      // +0x64
    TooMany = 19,      // "Save failed! There are too many save games."
    DeleteAsk = 20,    // delete the slot? (+0x6C)
    Deleted = 21,      // +0x70
    DeleteError = 22,  // +0x74
    Deleting = 23,     // "Deleting..."
    End = 24,          // +0x7C: an operation ended
    ListEnd = 25,      // +0x80
    Asking = 26,       // +0x84: an answer is waited for
};
inline constexpr size_t kFlagCount = 27;

// The jettext line a flag shows: the PC block's line k (kJetFlags + k).
constexpr size_t flagText(SaveFlag flag) {
    return kJetFlags + static_cast<size_t>(flag);
}

// What the windows make of the first 24 flags (0x006ED9F0, the driver window's copy 0x006EDA50).
enum class FlagKind : int32_t { State = 0, Message = 1, Question = 2 };
extern const std::array<FlagKind, 24> kFlagKinds;

// The manager's states (its +0x00), which a step answers with.
enum class DeviceState : int32_t { Idle = 0, List = 1, Read = 2, Write = 3 };

// What the windows ask of the device (the request block's +0x00): list the saves (the records: read them), write.
enum class Request : int32_t { None = -1, List = 0, Write = 1 };

// 0x00767FE8: the requests and the answers.
struct SaveRequest {
    Request request = Request::None;  // +0x00: none again after each step
    int32_t answer = -1;  // +0x04 (0x00767FEC): a question's answer, -1 none, 0 no, 1 yes; -1 after each step
    int32_t next = 0;     // +0x08 (0x00767FF0): read the slot of the list / leave the end; 0 after each step
    int32_t done = 0;     // +0x0C (0x00767FF4): the list is left; 0 after each step
    int32_t remove = 0;   // +0x10 (0x00767FF8): delete the slot; 0 after each step
    std::array<int32_t, 2> idleMarks{};       // +0x14, +0x18: 1 by the idle state (0x004FDF39), read by nothing
    std::array<int32_t, kFlagCount> flags{};  // +0x1C .. +0x84, by SaveFlag
    int32_t slot = -1;                        // +0x88 (0x00768070): the list's entry to read or delete
    float scale = 0.0f;                       // +0x8C: 1.0 after each step
    // +0x94 (0x0076807C), +0x98 (0x00768080), +0x9C (0x00768084): read by the windows, not written by the PC's device
    // (0 there): with skipCardQuestion 0 the save windows ask whether to save on the PS2's memory card (1994); the
    // size a save needs, shown with "insufficient space" (flag 4), and the free size (autosave writes again while it
    // is less than the size needed).
    int32_t skipCardQuestion = 0;
    int32_t neededKB = 0;
    int32_t freeKB = 0;

    int32_t& flag(SaveFlag f) { return flags[static_cast<size_t>(f)]; }
    int32_t flag(SaveFlag f) const { return flags[static_cast<size_t>(f)]; }

    // The windows' clear (0x00464830 and the others): no request, -1 answer and slot; 0 next, done and remove.
    void clear() {
        request = Request::None;
        done = 0;
        next = 0;
        answer = -1;
        remove = 0;
        slot = -1;
    }
};

// A driver of the list (0x40 bytes, the list of the device's +0x14).
struct SaveSlot {
    std::wstring name;      // +0x00: USERDATA's name
    std::wstring folder;    // +0x10: the folder's name
    uint32_t index = 0;     // +0x20: the place in the sorted list
    uint64_t written = 0;   // +0x28: USERDATA's last write (FILETIME); the list starts with the latest
    float value = 0.0f;     // +0x38: USERDATA's f32
};

// A replay of the list (0xB0 bytes, the list of the device's +0x18; 0x005026B0 reads its USERDATA: u32 cars,
// u32 type, char[32] track, char[32] route, char[17] date, wchar_t[16] driver, in that order).
struct ReplaySlot {
    int32_t type = 0;          // +0x00: the race's type
    int32_t cars = 0;          // +0x04: 0 a damaged replay
    uint32_t index = 0;        // +0x08: the place in the sorted list
    uint64_t written = 0;      // +0x10: USERDATA's last write (FILETIME); the list starts with the latest
    std::string date;          // +0x18: "month/day/year hour:minute" as the replay was saved (17 bytes)
    std::string track;         // +0x29
    std::string route;         // +0x49
    std::wstring driver;       // +0x6A
    std::wstring folder;       // +0x8C: the folder's name, "R" and the time
};

// The files of a save (the device's +0x2C, 0x00501B40 opens one by name): written in the order of the names' bytes.
struct SaveSource {
    std::wstring folder;  // the folder of SAVE written
    std::map<std::string, std::vector<uint8_t>> files;
};

// What the device asks of the game.
class SaveGameSide {
public:
    virtual ~SaveGameSide() = default;
    // The files of the driver (0x004FA0E0 USERDATA, the save system's +0x04: juicey.dat) and its folder.
    virtual void buildProfile(SaveSource& source) = 0;
    // A driver read (0x004FA2B0: the save system's +0x20 name, +0x38 folder, +0x08 the files); false when its files
    // do not hold.
    virtual bool applyProfile(const SaveSlot& slot, const SaveSource& source) = 0;
    // The records' USERDATA (0x0040A370) and their reading (0x0040A440).
    virtual void buildHighScores(SaveSource& source) = 0;
    virtual bool applyHighScores(const SaveSource& source) = 0;
    // reg.cf1, written with each file (0x005A33A0) and read with each file of a driver (0x005A3410).
    virtual void writeRegCf1() = 0;
    virtual void readRegCf1() = 0;
};

// The replays' object of the replays' device (its +0x0C): +0x00 puts a replay's files into the source of a write
// with the date's text (0x004FA600; its answer is not looked at), +0x04 takes the source of a read (0x004FA810), false
// when it does not hold a replay.
class SaveReplays {
public:
    virtual ~SaveReplays() = default;
    virtual bool save(SaveSource& source, const std::string& date) = 0;
    virtual bool take(const SaveSource& source) = 0;
};

// The device's manager ([0x00728D98], 0x004FDB30): the states idle, list, read and write, a step a frame.
class SaveDevice {
public:
    enum class Kind : int32_t { Profile = 0, Replay = 1, HighScores = 2 };

    // The folder SAVE (the stock game's <AppData>\THQ\Juiced\SAVE).
    void setFolder(std::filesystem::path save) { m_save = std::move(save); }
    // The fixed build's folder of the replays (<My Documents>\THQ\Juiced\Replays, made when one is written): the
    // replays' device lists, reads, writes and deletes its folders there, not in SAVE; none, SAVE as the stock.
    void setReplayFolder(std::filesystem::path replays) { m_replayFolder = std::move(replays); }

    // The profile device's step ([game + 0x60] +0x08, 0x004FA430) and the records' ([game + 0xA4] +0x04, 0x004FAB20):
    // the source made or taken, the manager's step, a read applied; the state after it.
    DeviceState stepProfile(SaveRequest& request, SaveGameSide& game);
    DeviceState stepHighScores(SaveRequest& request, SaveGameSide& game);
    // The replays' device ([game + 0xA0] +0x08, 0x004FA860; made by 0x004FA8E0, vtable 0x006F6DC8): the manager's
    // kind 1; a write's source named "R" and the local time ("R%04d%02d%02d%02d%02d%02d"; the fixed build's
    // "Replay_" and the time as "%02d-%02d-%04d_%02d-%02d-%02d", day, month, year, hour, minute, second, "_2", "_3" ..
    // after it for a folder there already) with the date's text
    // "%d/%d/%02d %2d:%02d" (month, day, year, the hour past 12 less 12, minute) given to the replays' object
    // (0x004FA600); a read taken by it (0x004FA810), else the read counts as corrupt and the device starts again.
    DeviceState stepReplay(SaveRequest& request, SaveGameSide& game);
    // The replays' device's +0x10 (0x004FA8B0): its replays' object (+0x0C) given; none is refused (false, the one
    // given kept). +0x14 (0x004FA5D0) gives it back. Stock problem: nothing on the PC calls +0x10 (the device's
    // constructor 0x004FA8E0 does not set +0x0C either), so the stock game's write or read of a replay calls through
    // address 0; the port gives none either, and with none the write's source has no files
    // and a read counts as corrupt.
    bool setReplayObject(SaveReplays* replays);
    SaveReplays* replayObject() const { return m_replayObject; }
    // The devices' +0x18 / +0x08 (0x004FDEE0): every state's step back to 0, the idle state.
    void reset();

    // The list made by the last list (the device's +0x14; the menu of the driver window, 0x00467620).
    const std::vector<SaveSlot>& slots() const { return m_slots; }
    // The replays listed (the device's +0x18; 0x004FA5E0 gives them when there are any).
    const std::vector<ReplaySlot>& replays() const { return m_replays; }
    int32_t found() const { return m_found; }

private:
    // The device's +0x10 .. +0x2C as the states see them.
    struct Side {
        int32_t buildSource = 0;  // +0x1C (sub +0x0C): make the files to write
        int32_t emptySource = 0;  // +0x20 (sub +0x10): take an empty source to read into
        int32_t ready = 0;        // +0x24 (sub +0x14): the source was made this step
        Kind kind = Kind::Profile;  // +0x28 (sub +0x18)
    };

    DeviceState step(SaveRequest& request, Kind kind, SaveGameSide& game);
    // 0x004FDE50
    DeviceState managerStep(SaveRequest& request, SaveGameSide& game);
    DeviceState idle(SaveRequest& request);                       // 0x004FDF30
    DeviceState list(SaveRequest& request);                       // 0x004FDF70
    DeviceState read(SaveRequest& request, SaveGameSide& game);   // 0x004FE510
    DeviceState write(SaveRequest& request, SaveGameSide& game);  // 0x004FEA10
    std::wstring slotFolder(const SaveRequest& request) const;
    // The folder the kind's saves are in: the replays' folder for the replays when given, else SAVE.
    const std::filesystem::path& root() const;

    std::filesystem::path m_save;
    std::filesystem::path m_replayFolder;
    Side m_side;
    SaveReplays* m_replayObject = nullptr;  // the replays' device's +0x0C
    std::unique_ptr<SaveSource> m_source;  // the device's +0x2C
    DeviceState m_state = DeviceState::Idle;  // the manager's +0x00
    std::vector<SaveSlot> m_slots;         // +0x14
    std::vector<ReplaySlot> m_replays;     // +0x18
    int32_t m_found = 0;                   // +0x10

    // The list state (0x006F7020): +0x08 step, +0x0C none found, +0x10 a find going on, +0x14 sorted, +0x18 count.
    // Its steps: the first folder, the next one (a folder a step), the delete asked, the end (2 is not used).
    enum class ListStep : int32_t { First = 0, Next = 1, Delete = 3, End = 4 };
    struct ListState {
        ListStep step = ListStep::First;
        int32_t none = 0;
        int32_t finding = 0;
        int32_t sorted = 0;
        uint32_t count = 0;
        std::vector<std::wstring> names;  // the folders of SAVE in the order FindFirstFile / FindNextFile give them
        size_t at = 0;
        std::wstring current;  // +0x28
        std::vector<SaveSlot> slots;
        std::vector<ReplaySlot> replays;
    } m_list;
    // The read state (0x006F7018): +0x08 step, +0x0C read, +0x10 failed, +0x14 the file, +0x18 the files. Its steps:
    // the folder's files found, a file a step, the end.
    enum class ReadStep : int32_t { Find = 0, File = 1, End = 2 };
    struct ReadState {
        ReadStep step = ReadStep::Find;
        int32_t ok = 0;
        int32_t failed = 0;
        size_t at = 0;
        std::wstring folder;  // +0x28
        std::vector<std::wstring> files;
    } m_read;
    // The write state (0x006F7028): +0x08 step, +0x0C written, +0x10 the file. Its steps: the source made, the folder
    // looked for, overwrite asked, the files' start, the folder made, a file a step, the end.
    enum class WriteStep : int32_t {
        Source = 0,
        Folder = 1,
        Overwrite = 2,
        Begin = 3,
        MakeFolder = 4,
        Files = 5,
        End = 6
    };
    struct WriteState {
        WriteStep step = WriteStep::Source;
        int32_t ok = 0;
        size_t at = 0;
        std::wstring folder;
    } m_write;
};

}  // namespace oj::save
