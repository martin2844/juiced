// CMSAdvice: the advisor's help over a career window.

#include "Game/Frontend/Career/Advice.h"

#include "Game/Frontend/WindowTools.h"

#include <iterator>

namespace oj::frontend {

// CMSAdvice (0x004BFB60): by the advisor's advice (+0x1C, none past 12) Advice, Topic and Tip the advice's lines
// (1679, 1694 and 1709 + the advice; 1692, 1707 and 1722 for 12), each over several lines; the advice's crew (0 .. 7
// crews 1 .. 8, 9 the advisor's +0x68 unless -1, 12 the player's crew; none for 8, 10, 11) shows Logo (its logo of
// the flyers), Leader (its leader's picture) and LEADER_BORDER, else they are hidden; the No button highlighted.
void Advice::enter(CareerHost& host, ui::Screen& screen) {
    GameData* data = host.gameData();
    const TextTable& gameTexts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const Career& career = *data->career;
    const int32_t advice = career.advisor.advice;
    std::wstring advise;
    std::wstring topic;
    std::wstring tip;
    if (advice >= 0 && advice <= 12) {
        const auto k = static_cast<size_t>(advice == 12 ? 13 : advice);
        advise = gameTexts.at(1679 + k);
        topic = gameTexts.at(1694 + k);
        tip = gameTexts.at(1709 + k);
    }
    int32_t crew = -1;
    if (advice >= 0 && advice <= 7) crew = advice + 1;
    if (advice == 9) crew = career.advisor.crew;
    if (advice == 12) crew = 0;
    if (crew >= 0 && crew < 9) {
        screen.setTexture("Logo", career.flyers.crews[static_cast<size_t>(crew)].logo);
        screen.setHidden("Logo", false);
        const auto id = static_cast<size_t>(career.crews[static_cast<size_t>(crew)].id);
        if (id < std::size(kLeaderPictures)) screen.setTexture("Leader", kLeaderPictures[id]);
        screen.setHidden("Leader", false);
        screen.setHidden("LEADER_BORDER", false);
    } else {
        screen.setHidden("Logo", true);
        screen.setHidden("Leader", true);
        screen.setHidden("LEADER_BORDER", true);
    }
    const std::array<std::pair<const char*, const std::wstring*>, 3> texts = {
        std::pair{"Advice", &advise}, std::pair{"Topic", &topic}, std::pair{"Tip", &tip}};
    for (const auto& [name, text] : texts) {
        screen.setText(name, *text);
        screen.setMultiline(name, true);
    }
    screen.setCurrentButton("No");
}

// 0x004C0110: SELECT (CLICK) on No drops the advice (+0x1C 13, its event none); on the other the advice asks
// CMSMainMenu for its window: advices 7 and 9 the phone (+0x04), 12 the calendar (+0x08), 11 the dealership (+0x10),
// any other the calendar (+0x0C); the window then goes (mode 6).
void Advice::update(CareerHost& host, ui::Screen& screen, uint32_t event) {
    GameData* data = host.gameData();
    if (event != input::kSelect || data == nullptr || !data->career) return;
    Career& career = *data->career;
    host.sound("CLICK");
    if (_stricmp(screen.currentButton().c_str(), "No") == 0) {
        career.advisor.advice = 13;
        career.adviceEvent = nullptr;
    } else {
        const int32_t advice = career.advisor.advice;
        if (advice == 7 || advice == 9) {
            career.advisor.openPhone = 1;
        } else if (advice == 12) {
            career.advisor.openHosting = 1;
        } else if (advice == 11) {
            career.advisor.openDealer = 1;
        } else {
            career.advisor.openCalendar = 1;
        }
    }
    host.request("", SwitchMode::Pop);
}

}  // namespace oj::frontend
