#pragma once
// The betting window "character" (docs/spec/14-career-windows.md): the rival crew's leader talks in short movies
// (movies\<leader>\<leader>_<CLIP>.bik), the player answers through a menu and message windows, and the bet with the
// crew is made or turned down before the race.

#include "Core/Data/GameData.h"
#include "Core/Data/Language.h"
#include "Core/Base/Random.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oj::frontend {

// 0x004A4130: Cash the player's money, Date the calendar's day (0x004A68F0).
void careerCashTexts(ui::Screen& screen, const Career& career, int language, const TextTable& texts);

// Why a crew's respect changes with the bets (the game's lines kTextRespectReasons + reason; ChallengeCompleted its
// own line): 0x004BC7B0's kinds.
enum class RespectReason : int32_t {
    HighBet = 0,             // +20
    LowBet = 1,              // -25
    BackedOut = 2,           // -35 (a hosted event's: down to 980)
    WaitedTooLong = 3,       // -30
    BetAccepted = 4,         // +20
    RaiseAccepted = 5,       // +10
    RaiseDeclined = 6,       // +30
    LowerDeclined = 7,       // -25
    ChallengeCompleted = 8,  // +50
};

// 0x004BC7B0: a crew's respect changed by kind (0 or 4 +20, 1 or 7 -25, 2 -35 or with the flag 980 - its respect, 3
// -30, 5 +10, 6 +30, 8 +50; none past 8) with a comment (kind 8 "CHALLENGE COMPLETED", else the leader's name, not in
// language 4, and the kind's line 437 + kind) followed by "Respect" and the change; shown in CMSRespectMsg over the
// window, then the crew's respect the sum (not below 0, cut).
void crewRespect(CareerHost& host, Career& career, int crew, RespectReason kind, int flag, int language,
                 const TextTable& texts);

// The car views of the leader (CAR_CHARACTER) and the player (CAR_PLAYER) of the betting windows: the leader's crew's
// scheme on its cars (0x004A9080), each view (0x0045A2C0: the zoom 0.82, the shadow on) shows the crew leader's car
// (0x004B1730 / 0x004B17C0) turned 3.74 ([0x0072530C], [0x00725308]) not turning, the camera at (0, 110, -560)
// (0x0045B050), and the enter waits until both are in.
struct BetCarViews {
    std::unique_ptr<scene::CarView> leader;  // +0x44 / +0x10
    std::unique_ptr<scene::CarView> player;  // +0x30 / +0x0C
    void make(ui::Screen& screen, GameData& data, CareerHost& host, bool hide);
    void frame(CareerHost& host, CrtRandom& random);
    void free();
};

// The MOVIE element of the betting windows (+0x58 of the record 0x0071FA78, +0x14 of 0x0071FB70): a clip is played
// as 0x005AA300 inlined (the interface's movie given back, the name, played once, the volume 1), at the settings'
// +0x5C, and its hold taken off when it is already the interface's; 0x00435030 holds it (stepped twice) or lets it
// go; a clip has ended when the interface's movie is the element's and has finished (0x0055A530).
struct LeaderMovie {
    ui::Screen* screen = nullptr;
    int element = -1;
    void bind(ui::Screen& s);
    void play(CareerHost& host, const std::string& name);
    bool finished() const;
    void hold(bool on);
};

class CharacterScreen {
public:
    // 0x0042E610
    void enter(CareerHost& host, ui::Screen& screen);
    // 0x0042F030: Menu's highlighted item kept (+0x04, read by nothing), the sound of +0xD8 stopped (+0xD8 is only
    // ever 0: nothing), the front end's music back to the settings' volume; 0x0042F0C0: the movie held, MOVIE's volume
    // 0, the car views freed.
    void leave(CareerHost& host);
    // 0x004325D0; event: the window's event this frame (the frame time the host's).
    void update(CareerHost& host, ui::Screen& screen, uint32_t event);
private:
    std::string clipPrefix() const;  // "<leader>\<leader>"
    void setCash(ui::Screen& screen, int value);
    void showMenuAgain(ui::Screen& screen);
    void cashTexts(ui::Screen& screen, const GameData& data);
    std::wstring line(size_t index) const;
    void respect(CareerHost& host, Career& career, RespectReason kind);
    void phoneContact(CareerHost& host, Career& career);
    void pickClip(ui::Screen& screen, GameData& data, CrtRandom& random, CareerHost& host, std::string& clip);

    const TextTable* m_texts = nullptr;
    int m_language = 7;
    LeaderMovie m_movie;  // +0x58: the MOVIE element

    // The state record 0x0071FA78 (offsets of its fields).
    int32_t m_debugClip = 0;      // +0x0C: the test mode's count (career +0x64 set; read by nothing)
    int32_t m_inputOn = 0;        // +0x1C
    int32_t m_lowered = 0;        // +0x20: the leader took a lower bet
    int32_t m_raised = 0;         // +0x24: the leader took a higher bet
    int32_t m_refusedHigher = 0;  // +0x28: the leader turned a higher bet down
    int32_t m_lookPrice = 0;      // +0x2C: the price of a look under the bonnet, (crew + 2) x 500
    std::optional<Car> m_stake;   // +0x14: a pink slip's stake, the player's car out of the garage
    int32_t m_stakeBack = 0;      // +0x18: the stake goes back into the garage
    int32_t m_shownCash = 0;      // +0x64
    int32_t m_cash = 0;           // +0x68: the amount BET_CASH shows
    float m_skipFade = 0.0f;      // +0x6C: the black SKIPFADE box over the movie
    int32_t m_amountAnswer = 0;   // +0x78: the answer to "Place Bet?" for an amount of the list
    std::string m_leader;         // +0x7C: the leader's movie folder
    int32_t m_amountIndex = 0;    // +0xBC
    int32_t m_pinkSlip = 0;       // +0xC0
    int32_t m_waits = 0;          // +0xC4: wait clips since the last impatient one
    int32_t m_tries = 0;          // +0xC8: amounts the leader may still turn down before leaving
    BetKind m_kind = BetKind::Money;  // +0xCC: the bets' kind
    int32_t m_stage = 0;          // +0xD0: the step of the talk
    int32_t m_playing = 0;        // +0xD4: a clip of the step plays
    int32_t m_quitAsked = 0;      // +0xE4
    int32_t m_noAsked = 0;        // +0xE8
    int32_t m_pinkAsked = 0;      // +0xEC
    int32_t m_yesAsked = 0;       // +0xF0
    int32_t m_answer = -1;        // +0xF4
    BetCarViews m_views;
    static inline int32_t m_impatience = 1;  // 0x00715AB4: which impatient clip comes next (kept while the game runs)
};

// The window after a race with a bet, "characterPostRace" (the record 0x0071FB70, vtable 0x006EFAC8): the crew's
// leader wins or loses the bet in a movie, the pot is counted into the player's money.
class CharacterPostRace {
public:
    // 0x004337D0
    void enter(CareerHost& host, ui::Screen& screen);
    // 0x004344D0; event: the window's event this frame (the frame time the host's).
    void update(CareerHost& host, ui::Screen& screen, uint32_t event);
    // 0x00434380: the sound of +0x98 stopped (only ever 0: nothing), the front end's music back to the settings'
    // volume, ..
    void leave(CareerHost& host);
    BetCarViews m_views;

private:
    const TextTable* m_texts = nullptr;
    int m_language = 7;
    LeaderMovie m_movie;          // +0x14: the MOVIE element

    float m_skipFade = 0.0f;      // +0x1C: the black SKIPFADE box over the movie
    int32_t m_outro = 0;          // +0x38: the first event's _CHALLENGE_OUTRO1 played
    float m_pot = 0.0f;           // +0x3C: the pot left to count
    float m_step = 0.0f;          // +0x40: the pot's 0.05, counted each frame
    float m_shown = 0.0f;         // +0x44: the money shown
    float m_delay = 0.0f;         // +0x48: 3 s before the count
    float m_pinkDelay1 = 0.0f;    // +0x4C
    float m_pinkDelay2 = 0.0f;    // +0x50
    std::string m_leader;         // +0x54: the leader's movie folder
    int32_t m_playing = 0;        // +0x94: a clip plays
};

}  // namespace oj::frontend
