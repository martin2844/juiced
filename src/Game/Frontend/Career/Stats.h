#pragma once
// CMSStats: the career's history as a graph.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/Frontend/Garage/Graph.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace oj::frontend {

// The window of the record 0x00722954: +0x0C the highlighted button, +0x14 .. +0x34
// the series shown (the money, then crews 1 .. 8), kept from one visit to the next
// ([0x0079E868] set at the first).
class Stats {
public:
    // 0x004DF750
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004DFC10
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004DFD50
    void draw(CareerHost& host, ui::Screen& s);
    // 0x004DFBC0: the graph freed.
    void leave() { m_graph.reset(); }

private:
    // 0x004E0680: the money's scale: the history's greatest money (0x004BD970, cut) under 10 000 gives 10 000, under
    // 100 000 100 000, under 1 000 000 1 000 000, else 10 000 000.
    int32_t scale(CareerHost& host);
    // 0x004E03A0: Tick1 .. 9 shown for the series shown, else hidden; with the money shown CashTop the scale x 0.001
    // and CashMid x 0.0005 (cut, thousands marked) as "$<n>k" ("<n> $k" in French), shown; else both hidden.
    void showTexts(CareerHost& host, ui::Screen& s);
    // 0x004E0110: a line of the graph from (x0, y0) to (x1, y1), each held to 0 .. 1, at 56 + 530 x (of 640) and 480 -
    // (130 + 260 y) (of 480) of the screen; drawn at y, y + 1 and y + 2 in the colour and at x + 1, y + 3 in black.
    void line(CareerHost& host, float x0, float y0, float x1, float y1, uint32_t argb);

    std::string m_button;
    std::array<int32_t, 9> m_series{};
    bool m_made = false;
    std::unique_ptr<Graph> m_graph;  // +0x10: TEST, never drawn
};

}  // namespace oj::frontend
