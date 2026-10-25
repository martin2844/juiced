// CMSCalendar: the career's calendar, its days, events and flyers.

#include "Game/Frontend/Career/Calendar.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Career/HostMeeting.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace oj::frontend {

namespace {

int32_t g_hosting = 0;            // [0x00728C54]
// [0x0079E850]: the highlighted button the calendar saw last. Stock problem: a pointer to the window's element, which
// the front end frees before a race and makes again after it, so the first frame after a race read the freed
// button's name (a random crash); its name is kept here.
std::string g_lastButton;
float g_selectPulse = 1.0f;       // [0x00725600]
float g_adviceBlink = 0.0f;       // [0x0079E84C]

// A day button's name D<column>-<row>: its row's digit (the name's fourth character) within '0' .. '7'.
bool isDayButton(const std::string& name) {
    return name.size() > 3 && name[3] >= '0' && name[3] <= '7';
}

// 0x004B4060: a flyer text's colour: grey for a pink slip or a challenge, white for a prototype part event, red for
// a prototype cars' event, white for a sponsor's territory or make, else the crew's colour of the flyers (the
// player's chosen one for crew 0).
uint32_t flyerTextColour(const Flyers& flyers, const CalendarEvent& e) {
    if (e.pinkSlip != 0 || e.challenge != 0) return 0xFFD4D4D4;
    if (e.asksPrototype != 0) return 0xFFFFFFFF;
    if (e.extreme != 0) return 0xFFFF3232;
    if ((e.region != 0 && e.crew != 0) || (e.make != 0 && e.crew != 0)) return 0xFFFFFFFF;
    // Stock problem: a chosen logo past 20 reads the colour after the player's (the crews'); here within them.
    if (e.crew == 0) return flyers.playerColours[static_cast<size_t>(flyers.playerColours[20]) % 21];
    return flyers.crews[static_cast<size_t>(e.crew) % 9].colour;
}

// 0x004B4130: the class's colour: as the text's for the first cases, then by the crew (0 the player's colour, 2
// 0xFFEDA725, 4 0xFFCA2A2A, 5 0xFFD9D9D9, 6 0xFFF11F2A, 7 0xFFDFC070, else white; none past 8).
bool flyerClassColour(const Flyers& flyers, const CalendarEvent& e, uint32_t& colour) {
    if (e.pinkSlip != 0 || e.challenge != 0) {
        colour = 0xFFD4D4D4;
        return true;
    }
    if (e.asksPrototype != 0) {
        colour = 0xFFFFFFFF;
        return true;
    }
    if (e.extreme != 0) {
        colour = 0xFFFF3232;
        return true;
    }
    if ((e.region != 0 && e.crew != 0) || (e.make != 0 && e.crew != 0)) {
        colour = 0xFFFFFFFF;
        return true;
    }
    static constexpr uint32_t kCrews[9] = {0, 0xFFFFFFFF, 0xFFEDA725, 0xFFFFFFFF, 0xFFCA2A2A,
                                           0xFFD9D9D9, 0xFFF11F2A, 0xFFDFC070, 0xFFFFFFFF};
    if (e.crew < 0 || e.crew > 8) return false;
    colour = e.crew == 0 ? flyers.playerColours[static_cast<size_t>(flyers.playerColours[20]) % 21]
                         : kCrews[static_cast<size_t>(e.crew)];
    return true;
}

}  // namespace

// 0x004B3F10: the player's own flyer: FlyerMain ui_flyer_player1 .. 3 by the chosen logo (the flyers' +0x50),
// FlyerPlayerLogo the player's crew logo; EventFee, FlyerClass and FlyerBHP hidden, EventPlayerFee, FlyerPlayerClass
// and FlyerPlayerLogo shown.
void playerFlyer(ui::Screen& s, const Flyers& flyers, int main) {
    static constexpr int kKinds[19] = {2, 3, 3, 2, 2, 1, 2, 1, 3, 3, 3, 3, 2, 1, 2, 1, 1, 1, 2};
    const auto logo = static_cast<int32_t>(flyers.playerColours[20]);
    const int kind = logo >= 1 && logo <= 19 ? kKinds[logo - 1] : 1;
    if (main >= 0) s.setTexture(main, std::format("ui_flyer_player{}", kind));
    s.setTexture("FlyerPlayerLogo", flyers.crews[0].logo);
    hideAll(s, "EventFee");
    hideAll(s, "FlyerClass");
    hideAll(s, "FlyerBHP");
    s.show("EventPlayerFee", false);
    s.show("FlyerPlayerClass", false);
    s.show("FlyerPlayerLogo", false);
}

// 0x004B2E20: the event's flyer. The flyers' +0x50 the chosen logo; EventFee, FlyerClass and FlyerBHP shown and
// EventPlayerFee, FlyerPlayerClass and FlyerPlayerLogo hidden; FlyerMain the crew's flyer (crews 1 .. 8; else the
// player's, 0x004B3F10). A prototype part event (+0x70): ui_flyer_PartSponsor, FlyerMod the kind's part (+0x74),
// FlyerSponsor shown; else those hidden. FlyerCarMake, FlyerCarModel, FlyerCarTerritory, FlyerSpecial,
// FlyerSpecial2 and FlyerProt hidden; a pink slip (+0x03): ui_flyer_PinkSlip and FlyerProt "PINK SLIP RACE"; a
// crew's territory (+0x60) or make (+0x64): ui_flyer_CarSponsor and its flag or the make's logo; a crew's model
// (+0x68): ui_flyer_CarSponsor, FlyerSpecial2 the make and model and FlyerCarMake the make's logo. FlyerNoEntry and
// its texts hidden, then by the reason the player may not enter or only attend (0x004B7D80): the reason's line (red,
// or yellow when entering is still allowed; reason 4 also with the line of reason 0), what car is needed for reason
// 1, the crew's line for reason 3. A prototype part event: FlyerSpecial "SPONSORED EVENT" and FlyerSpecial2 "Win
// Prototype Mods!"; a prototype cars' event (+0x04): ui_flyer_Prototype, FlyerProt "PROTOTYPE RACE" and FlyerSpecial2
// "Prototype Cars Only". FlyerRaceType the type's line; FlyerBHP the class's ratings ("<low>-<high> bhp"; hidden for
// class 8); FlyerClass and FlyerPlayerClass the class's line (FlyerClass hidden for a cruise or a challenge, shown
// for a crew's event; FlyerPlayerClass in the chosen colour, white for black); FlyerTeam the round's drivers' line
// (hidden for one); FlyerLaps the laps and the length (km, or miles x 0.625 with the settings' +0x40), only the length
// for types 3 and 5, "Perform flashy moves and tricks!" for a cruise; EventFee and EventPlayerFee the fee or "Free".
// The texts take the flyer's colours (0x004B4060, 0x004B4130).
void eventFlyer(WindowHost& host, ui::Screen& s, Career& career, const CalendarEvent& e) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Flyers& flyers = career.flyers;
    const PartCatalogue& catalogue = data->catalogue;
    const int language = host.language().index;
    flyers.playerColours[20] = host.settingsValue(save::Settings::kLogo);
    const uint32_t textColour = flyerTextColour(flyers, e);
    const auto colour = [&](int el) {
        if (el >= 0) s.setFontColour(el, textColour);
    };
    const auto line = [&](size_t index) { return texts.at(index); };
    const auto makeLogo = [](int make) {
        return make >= 1 && make <= 17 ? kMakeLogos[static_cast<size_t>(make)] : kMakeLogos[0];
    };
    s.show("EventFee", false);
    s.show("FlyerClass", false);
    s.show("FlyerBHP", false);
    hideAll(s, "EventPlayerFee");
    hideAll(s, "FlyerPlayerClass");
    hideAll(s, "FlyerPlayerLogo");
    const int main = s.find("FlyerMain");
    static constexpr const char* kFlyers[8] = {"ui_flyer_UrbanMaulerz", "ui_flyer_AWB",        "ui_flyer_Vixens",
                                               "ui_flyer_WildCats",     "ui_flyer_OmegaTau",   "ui_flyer_UrbanChaos",
                                               "ui_flyer_Lords",        "ui_flyer_Legion"};
    if (e.crew >= 1 && e.crew <= 8) {
        if (main >= 0) s.setTexture(main, kFlyers[e.crew - 1]);
    } else {
        playerFlyer(s, flyers, main);
    }
    if (e.asksPrototype != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_PartSponsor");
        static constexpr const char* kParts[10] = {"ui_part_suspension", "ui_part_backbox", "ui_part_nitro",
                                                   "ui_part_turbo",      "ui_part_",        "ui_part_brakes",
                                                   "ui_part_tyres",      "ui_part_",        "ui_part_airfilter",
                                                   "ui_part_"};
        const int mod = s.find("FlyerMod");
        if (mod >= 0) {
            s.setTexture(mod, kParts[e.prototypeKind >= 1 && e.prototypeKind <= 9 ? e.prototypeKind : 0]);
            s.setHidden(mod, false);
        }
        s.show("FlyerSponsor", false);
    } else {
        hideAll(s, "FlyerSponsor");
        hideAll(s, "FlyerMod");
    }
    for (const char* name : {"FlyerCarMake", "FlyerCarModel", "FlyerCarTerritory", "FlyerSpecial", "FlyerSpecial2",
                             "FlyerProt"}) {
        hideAll(s, name);
    }
    if (e.pinkSlip != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_PinkSlip");
        if (const int prot = s.find("FlyerProt"); prot >= 0) {
            s.setText(prot, line(kTextPinkSlipRace));
            s.setHidden(prot, false);
            colour(prot);
        }
    }
    if (e.region != 0 && e.crew != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_CarSponsor");
        const int territory = s.find("FlyerCarTerritory");
        s.show("FlyerCarTerritory", false);
        const char* flag = e.region == 2 ? "ui_flag_usa" : e.region == 3 ? "ui_flag_euro"
                                                          : e.region == 4 ? "ui_flag_oz" : "ui_flag_japan";
        if (territory >= 0) s.setTexture(territory, flag);
    }
    if (e.make != 0 && e.crew != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_CarSponsor");
        const int make = s.find("FlyerCarMake");
        s.show("FlyerCarMake", false);
        if (make >= 0) s.setTexture(make, makeLogo(e.make));
    }
    if (e.model != 0 && e.crew != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_CarSponsor");
        const int special = s.find("FlyerSpecial2");
        if (special >= 0) {
            s.setMultiline("FlyerSpecial2", true);
            colour(special);
        }
        s.show("FlyerSpecial2", false);
        const int make = modelMake(e.model);
        if (special >= 0) {
            s.setText(special, line(kTextMakes + static_cast<size_t>(make)) + L" " +
                                   line(kTextModels + static_cast<size_t>(std::max(e.model, 0))));
        }
        const int makeElement = s.find("FlyerCarMake");
        s.show("FlyerCarMake", false);
        if (makeElement >= 0) s.setTexture(makeElement, makeLogo(make));
    }
    hideAll(s, "FlyerNoEntry");
    hideAll(s, "FlyerNoEntryText");
    hideAll(s, "FlyerNoEntryText2");
    int reason = 0;
    if (mayEnter(career, e, reason, catalogue)) {
        if (reason == 0 || reason == 1 || reason == 7 || reason == 4) {
            s.show("FlyerNoEntry", false);
            s.show("FlyerNoEntryText", false);
            if (const int text = s.find("FlyerNoEntryText"); text >= 0) {
                std::wstring t = line(kTextNoEntry + static_cast<size_t>(reason));
                if (reason == 4) t += toWide("  ") + line(kTextNoEntry);
                s.setText(text, t);
                s.setMultiline("FlyerNoEntryText", true);
                s.setFontColour(text, 0xFFECEC00);
            }
        }
        if (reason == 1) {
            if (const int text = s.find("FlyerNoEntryText2"); text >= 0) {
                std::wstring t;
                if (e.region != 0) {
                    t = line(kTextNeedRegion);
                } else if (e.make != 0) {
                    t = line(kTextNeedMake);
                } else {
                    t = line(kTextNeedClass) + toWide(std::format("{}", e.carClass + 1));
                }
                s.setText(text, t);
                s.setMultiline("FlyerNoEntryText2", true);
                s.setHidden(text, false);
                s.setFontColour(text, 0xFFECEC00);
            }
        }
    } else {
        s.show("FlyerNoEntry", false);
        s.show("FlyerNoEntryText", false);
        if (const int text = s.find("FlyerNoEntryText"); text >= 0) {
            s.setMultiline("FlyerNoEntryText", true);
            s.setText(text, line(kTextNoEntry + static_cast<size_t>(reason)));
            s.setFontColour(text, 0xFFFF3232);
        }
        if (reason == 3) {
            if (const int text = s.find("FlyerNoEntryText2"); text >= 0) {
                s.setMultiline("FlyerNoEntryText2", true);
                s.setText(text, line(kTextNoEntryCrew + static_cast<size_t>(std::max<int>(e.crew, 0))));
                s.setHidden(text, false);
                s.setFontColour(text, 0xFFFF3232);
            }
        }
    }
    if (e.asksPrototype != 0) {
        if (const int special = s.find("FlyerSpecial"); special >= 0) {
            s.setText(special, line(kTextSponsored));
            s.setHidden(special, false);
            colour(special);
            s.setMultiline("FlyerSpecial", true);
        }
        if (const int special = s.find("FlyerSpecial2"); special >= 0) {
            s.setText(special, line(kTextWinPrototypes));
            s.setHidden(special, false);
            s.setMultiline("FlyerSpecial2", true);
            colour(special);
        }
    }
    if (e.extreme != 0) {
        if (main >= 0) s.setTexture(main, "ui_flyer_Prototype");
        if (const int prot = s.find("FlyerProt"); prot >= 0) {
            s.setText(prot, line(kTextPrototypeRace));
            s.setHidden(prot, false);
            colour(prot);
        }
        if (const int special = s.find("FlyerSpecial2"); special >= 0) {
            s.setText(special, line(kTextPrototypeCars));
            s.setHidden(special, false);
            s.setMultiline("FlyerSpecial2", true);
            colour(special);
        }
    }
    if (const int type = s.find("FlyerRaceType"); type >= 0) {
        s.setText(type, line(kTextRoundTypesUpper + static_cast<size_t>(std::max(static_cast<int32_t>(e.type), 0))));
        colour(type);
    }
    if (const int bhp = s.find("FlyerBHP"); bhp >= 0) {
        colour(bhp);
        if (e.carClass != 8) {
            int low = 0;
            int high = 0;
            classRatings(e.carClass, low, high);
            s.setText(bhp, toWide(std::format("{}-{} bhp", low, high)));
        } else {
            s.setHidden(bhp, true);
        }
    }
    const std::wstring className = line(kTextClasses + static_cast<size_t>(std::max(e.carClass, 0)));
    if (const int cls = s.find("FlyerClass"); cls >= 0) {
        uint32_t c = 0;
        if (flyerClassColour(flyers, e, c)) s.setFontColour(cls, c);
        s.setText(cls, className);
        if (e.type == EventKind::Showoff || e.challenge != 0) {
            s.setHidden(cls, true);
        } else if (e.crew != 0) {
            s.setHidden(cls, false);
        }
    }
    if (const int cls = s.find("FlyerPlayerClass"); cls >= 0) {
        const uint32_t chosen = flyers.playerColours[static_cast<size_t>(flyers.playerColours[20]) % 21];
        s.setFontColour(cls, (chosen & 0x00FFFFFFu) == 0 ? 0xFFFFFFFFu : chosen);
        s.setText(cls, className);
    }
    if (const int team = s.find("FlyerTeam"); team >= 0) {
        colour(team);
        if (e.round.teamSize > 1) {
            s.setText(team, line(kTextTeamSizes + static_cast<size_t>(e.round.teamSize)));
        } else {
            s.setHidden(team, true);
        }
    }
    if (const int laps = s.find("FlyerLaps"); laps >= 0) {
        s.setMultiline("FlyerLaps", false);
        colour(laps);
        const bool miles = host.settingsValue(save::Settings::kMph) != 0;
        std::wstring t;
        if (e.type == EventKind::Sprint || e.type == EventKind::PointToPoint) {
            t = toWide(miles ? std::format("{:.1f}m", e.round.km * 0.625f) : std::format("{:.1f}km", e.round.km));
        } else {
            t = line(kTextLaps) +
                toWide(miles ? std::format(" {} ({:.1f}m)", e.round.laps, e.round.km * 0.625f)
                             : std::format(" {} ({:.1f}km)", e.round.laps, e.round.km));
        }
        s.setText(laps, t);
        if (e.type == EventKind::Showoff) {
            s.setText(laps, line(kTextCruiseLaps));
            s.setMultiline("FlyerLaps", true);
        }
    }
    for (const char* name : {"EventFee", "EventPlayerFee"}) {
        if (const int fee = s.find(name); fee >= 0) {
            colour(fee);
            s.setText(fee, e.entranceFee == 0 ? line(kTextFree) : toWide(moneyText(e.entranceFee, language)));
        }
    }
}

// 0x004C2570: the crew's details (none for the player's crew): CD_ and CrewInfoBG shown; CD_Leader the leader's
// picture, CD_Name the leader's short name, CD_Crew the crew's name, CD_Location the location of its first circuit's
// route, CD_Respects what it respects most (0x004BC110), CD_respect_meter its respect / 1500 (at most 1).
void crewDetails(WindowHost& host, ui::Screen& s, const Career& career, int crew) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (crew == 0 || crew < 0 || crew >= 9) return;
    const Crew& c = career.crews[static_cast<size_t>(crew)];
    s.showContaining("CD_", false);
    s.show("CrewInfoBG", false);
    const auto id = static_cast<size_t>(c.id);
    if (id < std::size(kLeaderPictures)) s.setTexture("CD_Leader", kLeaderPictures[id]);
    const auto& name = c.memberShortNames[0];
    s.setText("CD_Name", toWide(std::string(name.data(), strnlen(name.data(), name.size()))));
    s.setText("CD_Crew", texts.at(kTextCrewNames + static_cast<size_t>(crew)));
    const std::vector<ExeTables::Route>& routes = data->tables.routes;
    const auto route = static_cast<size_t>(c.circuitRoute[0]);
    const int location = route < routes.size() ? routes[route].location : 0;
    s.setText("CD_Location", texts.at(kTextLocations + static_cast<size_t>(std::max(location, 0))));
    s.setText("CD_Respects", texts.at(kTextRespected + static_cast<size_t>(respectedMost(c))));
    s.setMeter("CD_respect_meter", std::min(1.0f, static_cast<float>(c.respect) * 0.00066666666f));
}

// 0x004C1A20: the calendar's five weeks from its first day (the record's +0x0C): the selection, the hosting and the
// flyer cleared (FLYER and the crew's details hidden), EnterEventText hidden when a day is highlighted, HostEventText
// hidden, Sel_but hidden when a day is highlighted; Cash and Date; EventFee and EventPlayerFee emptied; then each
// button D<column>-<row> its day's number, in white (odd months) or blue, grey on black for a past day (a past day
// past the first row moves the calendar on, +0x14); an event's day by the player's entry (attended white, closed dark
// red, attend only or not enough money yellow, open green); the advice's event's button kept (+0x1C) and, with the
// advice just given, highlighted. The highlighted day shows its event's flyer (0x004B2E20), its crew's details
// (0x004C2570) and EnterEventText when the player may enter; a later free day with no event of the player's crew
// waiting and no special event waiting offers hosting (HostEventText, FLYERNOENTRYTEXT2). Today is blue and, when
// asked, highlighted. Month_Year the highlighted day's month and year.
void CalendarWindow::build(CareerHost& host, ui::Screen& s, bool today) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Calendar& calendar = career.calendar;
    const PartCatalogue& catalogue = data->catalogue;
    const int curDay = calendar.dayOfMonth;
    const int curMonth = calendar.month;
    g_selectedDay = -1;
    g_selectedMonth = -1;
    g_hosting = 0;
    s.hideContaining("FLYER");
    s.hideContaining("CD_");
    hideAll(s, "CrewInfoBG");
    const bool dayHighlighted = isDayButton(s.currentButton());
    if (dayHighlighted) hideAll(s, "EnterEventText");
    hideAll(s, "HostEventText");
    s.setHidden("Sel_but", dayHighlighted);
    careerCashTexts(s, career, host.language().index, texts);
    if (const int e = s.find("EventFee"); e >= 0) s.clearText(e);
    if (const int e = s.find("EventPlayerFee"); e >= 0) s.clearText(e);
    m_adviceButton = -1;
    int labelMonth = 0;
    int dayIndex = m_start;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 7; ++col) {
            int month = 0;
            int day = 0;
            for (int k = 0; k < dayIndex; ++k) {
                ++day;
                if (day >= monthDays(month)) {
                    day = 0;
                    ++month;
                }
            }
            ++dayIndex;
            const std::string name = std::format("D{}-{}", col, row);
            const int el = s.find(name);
            if (el < 0) continue;
            s.setText(el, toWide(std::format("{}", day + 1)));
            const bool highlighted = _stricmp(name.c_str(), s.currentButton().c_str()) == 0;
            if (highlighted) labelMonth = month;
            s.setFontColour(el, (month & 1) != 0 ? 0xFAFFFFFFu : 0xFA9BD0E2u);
            s.setColour(el, 0x329BD0E2u);
            if (month < curMonth || (month == curMonth && day < curDay)) {
                s.setFontColour(el, 0xFA373737u);
                s.setColour(el, 0xFA000000u);
                if (row > 0) m_advance = 1;
            }
            CalendarEvent* e = eventAt(calendar, day, month);
            if (e != nullptr) {
                int reason = 0;
                if (e->attend[0] != 0) {
                    s.setColour(el, 0x9BFFFFFFu);
                } else if (!mayEnter(career, *e, reason, catalogue)) {
                    s.setColour(el, 0x9B7D0000u);
                } else if (reason == 0 || reason == 1 || reason == 7 || reason == 4) {
                    s.setColour(el, 0x9BD2D200u);
                } else {
                    s.setColour(el, 0x9B32FF32u);
                }
                if (e == career.adviceEvent) {
                    m_adviceButton = el;
                    if (career.advisor.chosen != 0) s.setCurrentButton(name);
                    career.advisor.chosen = 0;
                }
                if (_stricmp(name.c_str(), s.currentButton().c_str()) == 0) {
                    g_selectedDay = day;
                    g_selectedMonth = month;
                    s.showContaining("FLYER", false);
                    eventFlyer(host, s, career, *e);
                    crewDetails(host, s, career, e->crew);
                    if (mayEnter(career, *e, reason, catalogue)) {
                        s.show("EnterEventText", false);
                        s.setHidden("Sel_but", false);
                    }
                }
            } else if (highlighted) {
                bool ownWaiting = false;
                for (const CalendarEvent& o : calendar.events) {
                    if (o.used != 0 && o.crew == 0 && o.done == 0) ownWaiting = true;
                }
                const bool later = month > curMonth || (month == curMonth && day > curDay);
                if (!ownWaiting && !anyImportant(calendar) && later) {
                    s.show("HostEventText", false);
                    s.setHidden("Sel_but", false);
                    if (const int text = s.find("FLYERNOENTRYTEXT2"); text >= 0) {
                        s.setHidden(text, false);
                        s.setText(text, texts.at(kTextHostHere));
                        s.setFontColour(text, 0xFFFFFFFFu);
                        s.setMultiline("FLYERNOENTRYTEXT2", true);
                    }
                    g_hosting = 1;
                    g_selectedDay = day;
                    g_selectedMonth = month;
                }
            }
            if (day == curDay && month == curMonth) {
                s.setColour(el, 0x9B00B4FFu);
                if (today) s.setCurrentButton(name);
            }
        }
    }
    if (isDayButton(s.currentButton())) {
        const int m = labelMonth % 12;
        s.setText("Month_Year", texts.at(kTextMonths + static_cast<size_t>(m < 0 ? 0 : m)) + L" " +
                                    texts.at(kTextYear) + toWide(std::format(" {}", labelMonth / 12 + 1)));
    } else if (const int e = s.find("Month_Year"); e >= 0) {
        s.clearText(e);
    }
}

// 0x004C2490: the advice's day brought into the calendar: for the hosting advice (12) the first free day
// (0x004A6A10), else the advice's event's day; the first day moves on a week at a time until that day is within its
// first 14 days; for hosting that day is selected.
void CalendarWindow::toAdvice(CareerHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    int day = 0;
    int month = 0;
    if (career.advisor.advice == 12) {
        nextFreeDay(career.calendar, day, month);
    } else if (career.adviceEvent != nullptr) {
        day = career.adviceEvent->dayOfMonth;
        month = career.adviceEvent->month;
    } else {
        return;
    }
    int days = 0;
    for (int m = 0; m < month; ++m) days += monthDays(m);
    days += day;
    while (m_start + 14 < days) m_start += 7;
    if (career.advisor.advice == 12) {
        g_selectedDay = day;
        g_selectedMonth = month;
    }
}

// 0x004C11D0: the calendar filled up (0x004A2670); +0x18 -1, +0x1C and +0x14 0; the profile's +0x4E0 set; the first
// day the week's of today unless the calendar has one and the career is not new (the calendar's +0xF88, cleared);
// may scroll; the weeks built with today highlighted (0x004C1A20). Once the career is complete (0x004BCA70, with the
// profile's +0x08 0): the profile's +0x08 set, a note "Congratulations! ...", every car model's parts state 3 (the
// profile's +0x50 .. +0x261) and the extreme nitrous championships opened (0x0042B650). SELECT shown at the
// highlighted button's place.
void CalendarWindow::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Calendar& calendar = career.calendar;
    CareerProfile& p = career.profile;
    data->topUpCalendar(gameRandom());
    m_answer = -1;
    m_adviceButton = -1;
    m_advance = 0;
    p.raceCarKept = 1;
    if (m_start == 0 || calendar.newCareer != 0) m_start = calendar.daysPlayed / 7 * 7;
    calendar.newCareer = 0;
    m_mayScroll = 1;
    build(host, s, true);
    if (p.completeShown == 0 && !(careerCompletion(career) < 1.0f)) {
        p.completeShown = 1;
        host.message(MessageType::Continue, texts.at(kTextComplete), false, nullptr);
        for (size_t model = 0; model < data->tables.carModels.size(); ++model) p.performance(model).fill(3);
        data->arcade.openExtreme();
    }
    m_select = s.find("SELECT");
    m_x = m_y = m_targetX = m_targetY = 0.0f;
    const int current = s.find(s.currentButton());
    if (current >= 0 && m_select >= 0) {
        s.setHidden(m_select, false);
        float cw = 0.0f, ch = 0.0f;
        s.placeOf(current, m_targetX, m_targetY, cw, ch);
        m_x = m_targetX;
        m_y = m_targetY;
    }
}

// 0x004C1320: by the game's frame time: SELECT's alpha the pulse (|p|, p falling by 4 a second from 1 to -1) and its
// place easing (0.21 a frame) to the highlighted button's; the phone's frame (0x004B60F0). The advisor
// asks for the phone (+0x04) or for hosting (+0x08: the advice's day, CMSHostMeetingSetup with the selected day as the
// calendar's +0xF8C, +0xF90); a due challenge's advice with the profile's +0x4E over 3 (0x004A57E0, CMSAdvice over
// this one). BACK: CMSMainMenu. A new highlighted button builds the weeks again (its place taken at once when it
// wrapped across the row's ends). Scrolling: UP on row 0 a week back (not before today's week), DOWN on row 4 a week on
// (not past 25 days after today), each only once the highlight has rested there (+0x10); a past day past the first
// row (+0x14) moves on a week. PGDN: the window's help (0x004850E0 forced). Event 7 passes a day (0x004A6530)
// unless today's event is a special one waiting ("You are trying to skip past a special event in the calendar.").
// SELECT on an event's day the player may enter: past a special event the same note, else "GO TO EVENT?" (to +0x18);
// on a day to host CMSHostMeetingSetup. A "yes": the event chosen (the profile's +0x610), the meeting (0x004C27A0),
// CMSAtMeeting. The advice just given brings its day in (0x004C2490); the advice's button blinks (0.25 s blue, 0.25 s
// white).
void CalendarWindow::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Calendar& calendar = career.calendar;
    CareerProfile& p = career.profile;
    const float dt = host.gameFrameTime();
    if (m_select >= 0) {
        s.setAlpha(m_select, std::fabs(g_selectPulse));
        g_selectPulse = g_selectPulse - 4.0f * dt;
        if (g_selectPulse < -1.0f) g_selectPulse = 1.0f;
        m_x = (m_targetX - m_x) * 0.21f + m_x;
        m_y = (m_targetY - m_y) * 0.21f + m_y;
        float x = 0.0f, y = 0.0f, cw = 0.0f, ch = 0.0f;
        s.placeOf(m_select, x, y, cw, ch);
        s.setPlace(m_select, m_x, m_y, cw, ch);
    }
    host.phoneFrame(s);
    if (career.advisor.openPhone != 0) {
        career.advisor.openPhone = 0;
        host.request("CMSMobilePhone", SwitchMode::Push);
        return;
    }
    if (career.advisor.openHosting != 0) {
        career.advisor.openHosting = 0;
        career.advisor.chosen = 0;
        toAdvice(host);
        calendar.chosenDayOfMonth = g_selectedDay;
        calendar.chosenMonth = g_selectedMonth;
        host.hostMeeting().askDefaults();
        host.request("CMSHostMeetingSetup", SwitchMode::Replace);
        return;
    }
    if (challengeAdvice(career) > 0) {
        career.advisor.challengeDue = 1;
        if (p.respectLevel > 3) {
            chooseAdvice(career, gameRandom(), data->catalogue);
            host.request("CMSAdvice", SwitchMode::Push);
            return;
        }
    } else {
        career.advisor.challengeDue = 0;
    }
    if (event == input::kBack) {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    const std::string current = s.currentButton();
    if (current != g_lastButton) {
        bool wrapped = false;
        if (!g_lastButton.empty() && g_lastButton.size() > 1 && current.size() > 1) {
            wrapped = (g_lastButton[1] == '0' && current[1] == '6') || (g_lastButton[1] == '6' && current[1] == '0');
        }
        build(host, s, false);
        if (const int el = s.find(s.currentButton()); el >= 0) {
            float cw = 0.0f, ch = 0.0f;
            s.placeOf(el, m_targetX, m_targetY, cw, ch);
        }
        g_lastButton = s.currentButton();
        if (wrapped) {
            m_x = m_targetX;
            m_y = m_targetY;
        }
    }
    bool down = false;
    if (m_mayScroll != 0) {
        const std::string& now = s.currentButton();
        if (now.size() > 3 && (now[3] == '3' || now[3] == '1')) m_mayScroll = 0;
        if (m_start < calendar.daysPlayed && event == input::kUp) m_mayScroll = 0;
        if (m_start > calendar.daysPlayed + 25 && event == input::kDown) {
            m_mayScroll = 0;
            down = true;
        }
    }
    const std::string& now = s.currentButton();
    const char row = now.size() > 3 ? now[3] : '\0';
    if (!down && event == input::kSelect2) {
        host.sound("CLICK");
        host.helpPage("CMSCalendar", true);
    } else if (down || event == input::kDown) {
        if (row == '4') {
            if (m_mayScroll != 0) {
                host.sound("CLICK");
                m_start += 7;
                build(host, s, false);
            }
            m_mayScroll = 1;
        }
    } else if (event == input::kUp) {
        if (row == '0') {
            if (m_mayScroll != 0) {
                host.sound("CLICK");
                m_start -= 7;
                if (m_start < 0) m_start = 0;
                build(host, s, false);
            }
            m_mayScroll = 1;
        }
    }
    if (m_advance != 0) {
        m_advance = 0;
        host.sound("CLICK");
        m_start += 7;
        build(host, s, true);
    }
    if (event == input::kSelect3) {
        host.sound("CLICK");
        const CalendarEvent* today = eventAt(calendar, calendar.dayOfMonth, calendar.month);
        if (today != nullptr && today->done == 0 &&
            (today->challenge != 0 || today->pinkSlip != 0 || today->hostedByPlayer != 0)) {
            host.message(MessageType::Continue, texts.at(kTextSkipSpecial), false, nullptr);
        } else {
            data->nextDay(gameRandom());
            career.advisor.advice = 13;
            career.adviceEvent = nullptr;
            build(host, s, false);
        }
    } else if (event == input::kSelect) {
        if (g_selectedDay != -1 && g_selectedMonth != -1) {
            CalendarEvent* e = eventAt(calendar, g_selectedDay, g_selectedMonth);
            int reason = 0;
            if (e != nullptr && mayEnter(career, *e, reason, data->catalogue)) {
                if (importantBefore(calendar, g_selectedDay, g_selectedMonth) != nullptr) {
                    host.message(MessageType::Continue, texts.at(kTextSkipSpecial), false, nullptr);
                } else {
                    host.message(MessageType::YesNo, texts.at(kTextGoToEvent), false, &m_answer, true);
                }
            }
        }
        if (g_hosting != 0) {
            calendar.chosenDayOfMonth = g_selectedDay;
            calendar.chosenMonth = g_selectedMonth;
            host.hostMeeting().askDefaults();
            host.request("CMSHostMeetingSetup", SwitchMode::Replace);
        }
    }
    if (m_answer == 1) {
        career.meetingEvent = eventAt(calendar, g_selectedDay, g_selectedMonth);
        host.goToMeeting();
        host.sound("CLICK");
        host.request("CMSAtMeeting", SwitchMode::Replace);
    }
    if (career.advisor.chosen != 0) {
        career.advisor.chosen = 0;
        toAdvice(host);
        build(host, s, false);
    }
    if (m_adviceButton >= 0) {
        g_adviceBlink = g_adviceBlink + dt;
        if (g_adviceBlink > 0.5f) {
            g_adviceBlink = 0.0f;
            s.setColour(m_adviceButton, 0xFF0064FFu);
        } else if (g_adviceBlink > 0.25f) {
            s.setColour(m_adviceButton, 0xFFFFFFFFu);
        } else {
            s.setColour(m_adviceButton, 0xFF0064FFu);
        }
    }
}

}  // namespace oj::frontend
