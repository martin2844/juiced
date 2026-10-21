#pragma once
// The race front end's save windows (docs/spec/22-race-hud.md): savehighscores over newhighscore_cms and savereplay
// over hud_replay, run each frame of the race front end in place of its window, with the save device of the game's
// data.

#include "Core/Data/GameData.h"
#include "Core/Data/Language.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Saves/SaveDevice.h"
#include "Game/UI/MessageWindow.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace oj {

// What the save windows ask of the race front end.
class RaceSaveHost {
public:
    virtual ~RaceSaveHost() = default;
    // 0x005A80E0: a message of the race front end, its number counted.
    virtual void raceMessage(ui::MessageType type, const std::wstring& text, int32_t* answer, bool startNo) = 0;
    // The message up (the window manager's +0x44), the number of the last one made (+0x108), the one up closed.
    virtual bool messageUp() const = 0;
    virtual int32_t lastMessageNumber() const = 0;
    virtual void closeMessage() = 0;
    // The window manager's +0x110: busy with the save device (no network message then).
    virtual void setSaveBusy(int32_t busy) = 0;
    // A line of scripts\jettext<language>.txt; the game's texts (none in a view without the HUD).
    virtual const std::wstring& jetText(size_t line) = 0;
    virtual const TextTable* gameTexts() const = 0;
};

// The game's side of the records' device: SAVE\HS\USERDATA from the records, reg.cf1 with each file.
struct RaceRecordsSide : save::SaveGameSide {
    GameData* data = nullptr;
    std::filesystem::path saveFolder;
    void buildProfile(save::SaveSource&) override {}
    bool applyProfile(const save::SaveSlot&, const save::SaveSource&) override { return false; }
    void buildHighScores(save::SaveSource& source) override {
        if (data == nullptr) return;
        source.folder = L"HS";
        source.files["USERDATA"] = data->highScores.write();
    }
    bool applyHighScores(const save::SaveSource&) override { return false; }
    void writeRegCf1() override {
        const std::vector<uint8_t> reg = graphicsRegCf1(graphicsConfig()).write();
        std::ofstream(saveFolder / L"reg.cf1", std::ios::binary)
            .write(reinterpret_cast<const char*>(reg.data()), static_cast<std::streamsize>(reg.size()));
    }
    void readRegCf1() override {}
};

// savehighscores over newhighscore_cms (record 0x007213C8: +0x0C closed, +0x10 written, +0x14 write again, +0x18
// a failure answered by the question, +0x20 the message up, +0x24 .. +0x3C the messages shown by number (the
// write error, formatting, the question, a state's note), +0x44 a frame count, +0x4C the question's answer).
class SaveHighScoresWindow {
public:
    bool isOpen() const { return m_open; }
    // 0x00465DD0: the requests cleared, 0x00465D60.
    void enter(GameData& data, const std::filesystem::path& saveFolder);
    // 0x00465E50. Written: the message up closed and the window closes itself (its exit 0x00465E20: the device started
    // again). Else the records' device steps (SAVE\HS written a file a step); its flags: 4 the space's note (the flag's
    // line, " At least ", the size, "KB", " are required", " to save the hi-scores." 1492), "next"; 10 its note once;
    // 12 written; 13 the note of 10 closed; 16 "Save failed! Data write error" (type 2) once, failed; 17 its note when
    // 10's is up, write again; 19 the stock's debug text, failed; 6 the format question, 7 "Different hi-score data
    // already exists. Do you want to overwrite?" (779, type 4); 2 (not on the PC) its note after 15 frames. Failed:
    // "Hi-score has not been saved. Do you want to continue without saving?" (1485, type 4, on NO): yes ends, no writes
    // again.
    void update(RaceSaveHost& host);

private:
    // 0x00465D60: +0x0C .. +0x1C 0, +0x4C -1, the records' device started again, +0x44 and +0x20 0, the shown
    // messages' numbers out of reach, the write asked (1).
    void again();

    GameData* m_data = nullptr;
    RaceRecordsSide m_side;
    bool m_open = false;
    int32_t m_closed = 0;
    int32_t m_written = 0;
    int32_t m_again = 0;
    int32_t m_failed = 0;
    int32_t m_current = 0;
    int32_t m_writeError = 0;
    int32_t m_formatting = 0;
    int32_t m_question = 0;
    int32_t m_stateNote = 0;
    uint32_t m_frames = 0;
    int32_t m_answer = -1;
};

// savereplay over hud_replay (record 0x00721338; asked by hud_replay's save, which hud_replay.ui has no item for
// on the PC): +0x0C the PS2 question's answer, +0x14 the write ended (-1 going on), +0x18 failed, +0x20 write
// again, +0x24 done, +0x28 closed, +0x2C (0: write; else list), +0x30 the message up, +0x34 .. +0x50 the messages
// shown by number (the question, formatting, a state's note, no space, the write error; the step waits while
// +0x50's is up, a number nothing sets), +0x58 a frame count.
class SaveReplayWindow {
public:
    bool isOpen() const { return m_open; }
    // 0x004653A0: +0x2C 0, 0x00465310, +0x5C 0, +0x50 and +0x40 out of reach.
    void enter(GameData& data, const std::filesystem::path& saveFolder);
    // 0x00465400. Done: savereplay closes itself (mode 6, its exit 0x004653D0: the device started again) and +0x28.
    // Else, write again asked: 0x00465310. Else the replays' device steps; failed: the PS2 question "Do you want to
    // save the replay on memory card (8MB) (for PlayStation 2) ..." (434, type 3, into +0x0C; Stock problem: a
    // console's text, asked again each frame while unanswered), then no ends it, yes writes again. Not failed: the
    // write ended (+0x14 not -1): 0 ends it, else +0x14 -1 and write again. Going on: with the message up the one
    // of +0x50 nothing; past the write state (3) without flag 2 failed; else the flags (0x006ED9F0):
    //   4: the space's note (jettext's flag line, line 4, the size, "KB", line 5, " to save this replay." 1491; type 2,
    //   into "next") once, the requests cleared; 9: busy; 10: "Saving data..." (type 0) once, busy; 12: "Save
    //   Successful" (type 1), done; 13: the note of 10 closed; 16: "Save failed! Data write error" (type 1, into
    //   "next") once, the requests cleared; 17: with the note of 10 up "Format failed!" (type 1), the requests
    //   cleared, "next", failed; 19: the stock's debug text, the requests cleared, "next", failed; 6: the format
    //   question (lines 3 and 6 of the flags, type 4, on NO, into the answer) once; the other questions: the requests
    //   cleared, the device started again and asked to write. Kind 0 (not 1 or 3): after 15 frames its note (type 2,
    //   into +0x18) once. The other flags of kind 1 end the stock game (0x0059AF30); nothing here.
    void update(RaceSaveHost& host);

private:
    // 0x00465310: the requests cleared, the replays' device started again, the request (+0x2C 0: write, else -1),
    // +0x18 .. +0x28 0, +0x0C .. +0x14 -1, the shown messages' numbers out of reach.
    void again();

    GameData* m_data = nullptr;
    RaceRecordsSide m_side;
    bool m_open = false;
    int32_t m_answer = -1;
    int32_t m_ended = -1;
    int32_t m_failed = 0;
    int32_t m_again = 0;
    int32_t m_done = 0;
    int32_t m_closed = 0;
    int32_t m_listing = 0;
    int32_t m_current = 0;
    int32_t m_question = 99999;
    int32_t m_formatting = 99997;
    int32_t m_stateNote = 99996;
    int32_t m_noSpace = 99995;
    int32_t m_writeError = 99994;
    int32_t m_pending = 99991;
    uint32_t m_frames = 0;
};

}  // namespace oj
