// CMSStats: the career's history as a graph.

#include "Game/Frontend/Career/Stats.h"

#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// The series' colours of CMSStats ([0x007A270C], made at its first draw): the money white, crews 1 .. 8.
constexpr uint32_t kStatsColours[9] = {0xFFFFFFFF, 0xFFFF3737, 0xFF37FF37, 0xFF9B9BFF, 0xFFFFFF37,
                                       0xFFFF37FF, 0xFF37FFFF, 0xFFFF9B9B, 0xFF9BFFFF};

}  // namespace

// 0x004DF750: the first time the series cleared and crew 1's shown; the SELECTION and TICK elements hidden; +0x0C 0.
// The graph TEST (+0x10, 0x004F8610: 50 points, running) with nine series CASH (0x004F8780): for each of the
// history's 50 places from its place (+0x708) each series' next point its value, then the point after (0x004F8C30);
// all 50 points drawn and its place (10, 10), though nothing draws it. The texts (0x004E03A0); Cash and Date.
void Stats::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    if (!m_made) {
        m_series.fill(0);
        m_made = true;
        m_series[1] = 1;
    }
    s.hideContaining("SELECTION");
    s.hideContaining("TICK");
    m_button.clear();
    const History& history = data->career->history;
    m_graph = std::make_unique<Graph>("TEST", 50);
    m_graph->running = !m_graph->running;
    for (int n = 0; n < 9; ++n) m_graph->addSeries("CASH");
    for (int32_t j = 0; j < 50; ++j) {
        for (size_t k = 0; k < 9; ++k) {
            if (!m_graph->running || m_graph->series.size() <= k) continue;
            const auto at = static_cast<size_t>((history.position + j) % 50);
            m_graph->series[k].values[m_graph->next] = history.series[k][at];
        }
        m_graph->advance();
    }
    m_graph->drawn = 50;
    m_graph->x = 10.0f;
    m_graph->y = 10.0f;
    showTexts(host, s);
    careerCashTexts(s, *data->career, host.language().index, texts);
}

// 0x004DFC10: a new highlighted button (+0x0C) hides every SELECTION and shows Selection<its name>. SELECT (CLICK)
// turns the series of the button's number (1 the money, 2 .. 9 crews 1 .. 8) over and writes the texts again; BACK
// (CLICK) CMSStatsMenu.
void Stats::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    const std::string current = s.currentButton();
    if (current != m_button) {
        m_button = current;
        s.hideContaining("SELECTION");
        s.show("Selection" + current, false);
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        const int n = std::atoi(current.c_str()) - 1;
        if (n >= 0 && n < 9) m_series[static_cast<size_t>(n)] = 1 - m_series[static_cast<size_t>(n)];
        showTexts(host, s);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("CMSStatsMenu", SwitchMode::Replace);
    }
}

// 0x004E0680: the money's scale: the history's greatest money (0x004BD970, cut) under 10 000 gives 10 000, under
// 100 000 100 000, under 1 000 000 1 000 000, else 10 000 000.
int32_t Stats::scale(CareerHost& host) {
    GameData* data = host.gameData();
    float most = 0.0f;
    for (float v : data->career->history.series[0]) {
        if (most < v) most = v;
    }
    const int32_t top = x87::chop(most);
    if (top < 10000) return 10000;
    if (top < 100000) return 100000;
    return top >= 1000000 ? 10000000 : 1000000;
}

// 0x004E03A0: Tick1 .. 9 shown for the series shown, else hidden; with the money shown CashTop the scale x 0.001 and
// CashMid x 0.0005 (cut, thousands marked) as "$<n>k" ("<n> $k" in French), shown; else both hidden.
void Stats::showTexts(CareerHost& host, ui::Screen& s) {
    for (size_t n = 0; n < 9; ++n) {
        const std::string name = std::format("Tick{}", n + 1);
        if (m_series[n] != 0) {
            s.show(name, false);
        } else {
            s.setHidden(name, true);
        }
    }
    if (m_series[0] == 0) {
        s.setHidden("CashTop", true);
        s.setHidden("CashMid", true);
        return;
    }
    const auto money = [&](float share) {
        const std::string n = groupThousands(x87::chop(static_cast<float>(scale(host)) * share), host.language().index);
        return toWide((host.language().index == 1 ? n + " $" : "$" + n) + "k");
    };
    s.setText("CashTop", money(0.001f));
    s.setHidden("CashTop", false);
    s.setText("CashMid", money(0.0005f));
    s.setHidden("CashMid", false);
}

// 0x004E0110: a line of the graph from (x0, y0) to (x1, y1), each held to 0 .. 1, at 56 + 530 x (of 640) and 480 -
// (130 + 260 y) (of 480) of the screen; drawn at y, y + 1 and y + 2 in the colour and at x + 1, y + 3 in black.
void Stats::line(CareerHost& host, float x0, float y0, float x1, float y1, uint32_t argb) {
    const auto unit = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    const auto w = static_cast<float>(host.device().width());
    const auto h = static_cast<float>(host.device().height());
    const float ax = (unit(x0) * 530.0f + 56.0f) * w * 0.0015625f;
    const float ay = (480.0f - (unit(y0) * 260.0f + 130.0f)) * h * 0.0020833334f;
    const float bx = (unit(x1) * 530.0f + 56.0f) * w * 0.0015625f;
    const float by = (480.0f - (unit(y1) * 260.0f + 130.0f)) * h * 0.0020833334f;
    host.device().drawLine(ax, ay, bx, by, argb);
    host.device().drawLine(ax, ay + 1.0f, bx, by + 1.0f, argb);
    host.device().drawLine(ax, ay + 2.0f, bx, by + 2.0f, argb);
    host.device().drawLine(ax + 1.0f, ay + 3.0f, bx + 1.0f, by + 3.0f, 0xFF000000);
}

// 0x004DFD50: the graph's frame in white; with a crew's series shown the respect steps' lines (100, 300, 600, 1000 of
// 1500) in 0xFF00B4FF and RESPTXT shown, else RESPTXT hidden; each series shown as 49 lines of 0.02 across the
// history's 50 places from its place (+0x708): the money over the scale (0x004E0680), a crew's respect / 1500.
void Stats::draw(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    const History& history = data->career->history;
    const uint32_t frame = kStatsColours[0];
    line(host, 0.0f, 0.0f, 1.0f, 0.0f, frame);
    line(host, 1.0f, 0.0f, 1.0f, 1.0f, frame);
    line(host, 1.0f, 1.0f, 0.0f, 1.0f, frame);
    line(host, 0.0f, 1.0f, 0.0f, 0.0f, frame);
    bool crews = false;
    for (size_t k = 1; k < 9; ++k) crews = crews || m_series[k] != 0;
    if (crews) {
        for (float step : {0.06666667f, 0.2f, 0.4f, 0.6666667f}) line(host, 0.0f, step, 1.0f, step, 0xFF00B4FF);
        s.showContaining("RESPTXT", false);
    } else {
        s.hideContaining("RESPTXT");
    }
    for (size_t k = 0; k < 9; ++k) {
        if (m_series[k] == 0) continue;
        float x = 0.0f;
        for (int32_t j = 0; j < 49; ++j) {
            const auto at = [&](int32_t i) {
                return history.series[k][static_cast<size_t>((history.position + i) % 50)];
            };
            float v0 = 0.0f;
            float v1 = 0.0f;
            if (k == 0) {
                v0 = at(j) / static_cast<float>(scale(host));
                v1 = at(j + 1) / static_cast<float>(scale(host));
            } else {
                v0 = at(j) * 0.00066666666f;
                v1 = at(j + 1) * 0.00066666666f;
            }
            const float next = x + 0.02f;
            line(host, x, v0, next, v1, kStatsColours[k]);
            x = next;
        }
    }
}

}  // namespace oj::frontend
