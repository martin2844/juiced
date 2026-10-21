#pragma once
// CMSGridWalk: the grid before a race and the bets on it (CMSBet over it).

#include "Core/Cars/Car.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj::frontend {

// The grid walk (CMSGridWalk, the record 0x00722148, vtable 0x006F55A8) and the bet over it
// (CMSBet, the record 0x007221B0, vtable 0x006F55C8): the grid walk sets the place bet on and
// the bet asks for the grid's lines again.
class GridWalk {
public:
    // 0x004C7FA0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C91B0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004C9130
    void leave();
    // 0x004CA1A0
    void enterBet(CareerHost& host, ui::Screen& s);
    // 0x004CA210
    void updateBet(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004C9DC0: the racers' lines: the blinking list emptied (+0x58); for each place %dInfo2 the crew's odds
    // ("%d:%d", with "($<money on them>)" when there is) for a crew's leader when betting on cars (the profile's
    // +0x61C, not +0x614), else emptied; the leader of the crew bet with shows the bet. bet<n> hidden, then shown and
    // blinking for a leader of another crew the player may still bet with when betting with drivers (the profile's
    // +0x614, no +0x02 or +0x03, no bet asked, no amount; its bets made, 0x004B1870, with their middle amount under the
    // money), and for a leader with no money on its odds when betting on cars.
    void lines(CareerHost& host, ui::Screen& s);
    // The SELECT<n> of the highlighted racer's button (its name's number) shown with the pulse, the others hidden; the
    // pulse; the lines again when asked (+0x5C).
    void selectMarkers(CareerHost& host, ui::Screen& s);
    // 0x004CA5F0: the racer's crew ([0x007221BC] the place): odds "ODDS" and its odds (0x004B9C50); cash "CASH" and the
    // money; amount "BET" and the amount; winnings (when the window has it) "POTENTIAL WIN" and $ the amount / the
    // stake x against, cut, thousands marked.
    void betTexts(CareerHost& host, ui::Screen& s);

    std::array<std::unique_ptr<scene::CarView>, 6> m_views;  // +0x0C: the racers' car views
    std::vector<Car> m_cars;            // +0x24: the cars' list (0x00489B80), copies of the racers' cars
    std::array<int, 6> m_select{-1, -1, -1, -1, -1, -1};  // +0x28: SELECT1 .. 6
    std::array<int, 6> m_blink{};       // +0x40: the bet elements blinking
    int32_t m_blinkCount = 0;           // +0x58
    int32_t m_refresh = 0;              // +0x5C: the lines again (0x004C9DC0)
    int32_t m_quitAnswer = 0;           // +0x60
    int32_t m_starting = 0;             // +0x64: the race starts on the next frame

    // The record of CMSBet (0x007221B0, vtable 0x006F55C8); its +0x0C is the place it bets on.
    struct Bet {
        int32_t amount = 0;   // +0x10
        float left = 0.0f;    // +0x14: the money left
        int32_t answer = 0;   // +0x18: "Place Bet?"
    };
    Bet m_bet;
    int32_t m_betRacer = 0;  // [0x007221BC] (the bet's +0x0C): the place CMSBet bets on
};

}  // namespace oj::frontend
