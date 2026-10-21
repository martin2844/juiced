// loadreplay: the replays listed, one read or deleted (no way leads to it on the PC).

#include "Game/Frontend/Saves/LoadReplay.h"

#include "Core/Data/GameTexts.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

using save::FlagKind;
using save::SaveFlag;

namespace {

// The PC block of jettext: the line of flag 0.

}  // namespace

// 0x004694B0: the windows' enter, the record cleared (0x004693A0), +0x38 0, LAPSTITLE "CARS" (65, its FontModifier
// started again).
void LoadReplay::enter(SaveHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    clear(host, s);
    m_noteShown = 0;
    s.setText("LAPSTITLE", texts.at(kTextCars));
}

// 0x004693A0: +0x0C, +0x10, +0x3C, +0x40 1; +0x14, +0x2C .. +0x34, +0x48 0; the messages' numbers out of reach;
// +0x44 -1; the requests cleared; MENU emptied (0x005AEEF0); every INFO_ and DEL_ element hidden; the list asked
// for (0) and the device started again ([game + 0xA0] +0x0C).
void LoadReplay::clear(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    m_cleared = 1;
    m_refill = 1;
    m_infoHidden = 1;
    m_infoState = 1;
    m_loaded = 0;
    m_leaving = 0;
    m_ready = 0;
    m_busy = 0;
    m_loading = 999999;
    m_checking = 999998;
    m_deleteAsked = 999997;
    m_deleting = 999996;
    m_networkNote = 999995;
    m_wait = 0.0f;
    m_shown = -1;
    if (s.find("menu") >= 0) s.clearMenu("menu");
    s.hideContaining("INFO_");
    s.hideContaining("DEL_");
    if (data == nullptr) return;
    data->saveRequest.clear();
    data->saveRequest.request = save::Request::List;
    data->saveDevice.reset();
}

// 0x00468FC0: MENU emptied, then each replay listed: one with cars its date's text (its place the value), else "DAMAGED
// REPLAY" (1665, the command "corrupt"); the DEL_ elements shown with any; the first item highlighted; +0x0C 0.
void LoadReplay::fill(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (s.find("menu") < 0 || data == nullptr) return;
    s.clearMenu("menu");
    const std::vector<save::ReplaySlot>& replays = data->saveDevice.replays();
    for (const save::ReplaySlot& r : replays) {
        if (r.cars > 0) {
            s.addMenuItem("menu", widenSigned(r.date), static_cast<float>(r.index));
        } else {
            s.addMenuItem("menu", texts.at(kTextDamagedReplay), static_cast<float>(r.index), "corrupt");
        }
    }
    if (!replays.empty()) s.showContaining("DEL_", false);
    s.selectMenuItem("menu", 0);
    m_cleared = 0;
}

// 0x004690E0: a replay's details, each text's FontModifier started again; a replay without cars shows empty texts.
// INFO_RACETRACK the city place of its track (0x00462780; the first place when none has the name); INFO_ROUTE empty
// for a showoff or a sprint (types 4 and 6), else its route's name (0x00426520: the first route of that name) or
// empty; INFO_RACETYPE the race type's name (495 + type; type 7 is first made 5, in the record too); INFO_NUMCARS the
// cars ("%d"); INFO_DRIVERNAME the driver; INFO_TRACK_PIC the place's picture (0x00462730: none when no place has the
// name), hidden for a replay without cars.
void LoadReplay::showInfo(SaveHost& host, ui::Screen& s, const save::ReplaySlot& replay) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const ExeTables& t = data->tables;
    const bool cars = replay.cars > 0;
    if (s.find("INFO_RACETRACK") >= 0) {
        std::wstring text;
        if (cars && !t.cityLocations.empty()) {
            text = t.cityLocations.front().text;
            for (const ExeTables::CityLocation& place : t.cityLocations) {
                if (_stricmp(place.name.c_str(), replay.track.c_str()) == 0) {
                    text = place.text;
                    break;
                }
            }
        }
        s.setText("INFO_RACETRACK", text);
    }
    if (s.find("INFO_ROUTE") >= 0) {
        std::wstring text;
        const auto raced = static_cast<RaceType>(replay.type);
        if (raced != RaceType::Cruise && raced != RaceType::Sprint && cars && !t.routes.empty()) {
            for (const ExeTables::Route& r : t.routes) {
                if (_stricmp(r.name.c_str(), replay.route.c_str()) == 0) {
                    if (r.text != 0) text = texts.at(frontend::routeNameLine(r.text, t.routes[0].text));
                    break;
                }
            }
        }
        s.setText("INFO_ROUTE", text);
    }
    if (s.find("INFO_RACETYPE") >= 0) {
        // A point-to-point is told as a circuit race.
        const int32_t type = replay.type == static_cast<int32_t>(RaceType::PointToPoint)
                                 ? static_cast<int32_t>(RaceType::Circuit)
                                 : replay.type;
        s.setText("INFO_RACETYPE", cars ? texts.at(kTextRaceTypes + static_cast<size_t>(type)) : L"");
    }
    if (s.find("INFO_NUMCARS") >= 0) s.setText("INFO_NUMCARS", cars ? std::to_wstring(replay.cars) : L"");
    if (s.find("INFO_DRIVERNAME") >= 0) s.setText("INFO_DRIVERNAME", cars ? replay.driver : L"");
    if (s.find("INFO_TRACK_PIC") >= 0) {
        s.setHidden("INFO_TRACK_PIC", !cars);
        if (cars) {
            for (const ExeTables::CityLocation& place : t.cityLocations) {
                if (_stricmp(place.name.c_str(), replay.track.c_str()) == 0) {
                    s.setTexture("INFO_TRACK_PIC", place.texture);
                    break;
                }
            }
        }
    }
}

// loadreplay's update (0x00469530; record 0x007215D0, no way leads to it on the PC: extras takes its replay item
// out). MENU takes events only with no message up; the window is busy (+0x110) under the loading and deleting notes.
// Not leaving: the checking's wait runs down; the INFO_ elements follow +0x3C; the replays' device steps; idle with a
// replay read (the replay object has taken it, 0x00429550: its race set, playing from the start, "menu" after the
// race) the window closes the loading note (+0x48), asks for loading (mode 1) and leaves, idle else the list is asked
// again. The flags (0x006EDA50):
//   0: busy; "Checking hard disk..." (type 0) once, its wait 1 s; "next" 0. 4: "next". 8: "Loading data..." (type 0)
//   once, busy, "next" 0. 5 (none), 14 (listed): once the checking note had its second it is closed (+0x44 -1 with
//   5) and +0x3C goes 1 (5) or 0 (14); ready, not busy; 14 fills MENU when asked (0x00468FC0, +0x44 999999).
//   11 read: +0x14, ready, not busy, "next". 15, 18: their note (type 1), not ready-read, "next", the record cleared
//   (0x004693A0), +0x4C 0.5. 21: the deleting note closed, MENU emptied, DEL_ hidden, "Delete successful!" (type 1),
//   the menu to be filled again. 22: its note (type 1), MENU emptied, to be filled again, +0x4C 0.5. 23: "Deleting..."
//   (type 0) once, busy. 20: "Are you sure you want to delete driver <the item>?" (type 4, on NO, into the answer)
//   once. 1 .. 3 (not on the PC): MENU emptied once to be filled again; 2 a note (type 2) once, the checking note
//   closed and +0x44 -1; 1 clears +0x38; ready.
// Then the highlighted replay's details when it changed (0x004690E0), else with +0x44 below 0 +0x3C 1. SELECT (ready,
// not busy, no message): its replay read ("next"), CLICK; SELECT2 likewise deletes it; BACK (not busy, no message):
// CLICK, extras (mode 1), leaving. Leaving closes a message up.
void LoadReplay::update(SaveHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    host.setSaveBusy(0);
    const int32_t current = host.messageNumber();
    if (current == m_loading || current == m_deleting) host.setSaveBusy(1);
    if (m_leaving != 0) {
        if (current != 0 && host.messageUp()) host.closeMessage();
        return;
    }
    const bool hasMenu = s.find("menu") >= 0;
    if (hasMenu) s.setMenuActive("menu", current == 0);
    if (m_wait > 0.0f) m_wait = m_wait - host.frameTime();
    if (m_infoState != m_infoHidden) {
        if (m_infoHidden == 0) {
            s.showContaining("INFO_", true);
        } else {
            s.hideContaining("INFO_", true);
        }
        m_infoState = m_infoHidden;
    }
    const save::DeviceState state = data->saveDevice.stepReplay(r, host.saveSide());
    if (state == save::DeviceState::Idle) {
        if (m_loaded != 0) {
            if (current == m_loading && host.messageUp()) host.closeMessage();
            host.request("loading", SwitchMode::Replace);
            m_leaving = 1;
            return;
        }
        r.request = save::Request::List;
    }
    for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
        if (r.flags[k] == 0) continue;
        const FlagKind kind = save::kFlagKinds[k];
        const SaveFlag flag = static_cast<SaveFlag>(k);
        if (kind == FlagKind::Message) {
            switch (flag) {
            case SaveFlag::Checking:
                host.setSaveBusy(1);
                if (current != m_checking) {
                    host.message(MessageType::Note, host.jetText(save::flagText(flag)), false, nullptr);
                    m_checking = host.lastMessageNumber();
                    m_wait = 1.0f;
                    m_busy = 1;
                }
                r.next = 0;
                break;
            case SaveFlag::NoSpace: r.next = 1; break;
            case SaveFlag::NoData:
            case SaveFlag::Listed:
                if (current == m_checking) {
                    if (m_wait > 0.0f) return;
                    if (host.messageUp()) host.closeMessage();
                    if (flag == SaveFlag::NoData) m_shown = -1;
                }
                m_infoHidden = flag == SaveFlag::NoData ? 1 : 0;
                m_ready = 1;
                m_busy = 0;
                if (flag == SaveFlag::Listed && m_refill != 0) {
                    fill(host, s);
                    m_refill = 0;
                    m_shown = 999999;
                }
                break;
            case SaveFlag::Loading:
                host.setSaveBusy(1);
                if (current != m_loading) {
                    host.message(MessageType::Note, host.jetText(save::flagText(flag)), false, nullptr);
                    m_loading = host.lastMessageNumber();
                    m_busy = 1;
                }
                r.next = 0;
                break;
            case SaveFlag::Loaded:
                m_loaded = 1;
                m_ready = 1;
                m_busy = 0;
                r.next = 1;
                host.setSaveBusy(0);
                break;
            case SaveFlag::ReadError:
            case SaveFlag::Corrupt:
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                m_loaded = 0;
                m_busy = 0;
                m_ready = 1;
                r.next = 1;
                host.setSaveBusy(0);
                clear(host, s);
                m_afterFailure = 0.5f;
                break;
            case SaveFlag::Deleted:
                if (current == m_deleting) {
                    if (host.messageUp()) host.closeMessage();
                    host.setSaveBusy(0);
                }
                if (hasMenu) s.clearMenu("menu");
                s.hideContaining("DEL_");
                host.message(MessageType::Continue, texts.at(kTextDeleted), false, nullptr);
                m_refill = 1;
                m_busy = 0;
                break;
            case SaveFlag::DeleteError:
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                if (hasMenu) s.clearMenu("menu");
                host.setSaveBusy(0);
                m_busy = 0;
                m_refill = 1;
                m_afterFailure = 0.5f;
                break;
            case SaveFlag::Deleting:
                host.setSaveBusy(1);
                if (current != m_deleting) {
                    host.message(MessageType::Note, host.jetText(save::flagText(flag)), false, nullptr);
                    m_deleting = host.lastMessageNumber();
                    m_busy = 1;
                }
                break;
            default:
                break;
            }
        } else if (kind == FlagKind::State) {
            if (flag == SaveFlag::Inserted) m_noteShown = 0;
            if (m_refill == 0 && flag != SaveFlag::Inserted) {
                if (hasMenu) s.clearMenu("menu");
                host.setSaveBusy(0);
                m_refill = 1;
            }
            if (flag == SaveFlag::NoDevice) {
                if (current != m_networkNote && m_noteShown == 0) {
                    host.message(MessageType::ContinueModeless, host.jetText(save::flagText(flag)), false, nullptr);
                    m_networkNote = host.lastMessageNumber();
                    m_noteShown = 1;
                }
                host.setSaveBusy(0);
                if (current == m_checking && host.messageUp()) host.closeMessage();
                m_shown = -1;
            }
            m_ready = 1;
        } else if (kind == FlagKind::Question && flag == SaveFlag::DeleteAsk && current != m_deleteAsked) {
            host.setSaveBusy(0);
            if (hasMenu && s.menuSize("menu") > 0) {
                const size_t item = s.selectedMenuItem("menu");
                host.message(MessageType::YesNoModeless,
                             host.jetText(save::flagText(flag)) + s.menuItemText("menu", item) + L"?", false, &r.answer,
                             true);
                m_deleteAsked = host.lastMessageNumber();
            }
        }
    }
    if (m_shown < 0) {
        if (m_infoHidden == 0) m_infoHidden = 1;
    } else if (hasMenu && s.menuSize("menu") > 0) {
        const auto place = static_cast<int32_t>(s.menuItemValue("menu", s.selectedMenuItem("menu")));
        if (place != m_shown) {
            m_shown = place;
            const std::vector<save::ReplaySlot>& replays = data->saveDevice.replays();
            if (!replays.empty() && place >= 0 && static_cast<size_t>(place) < replays.size()) {
                showInfo(host, s, replays[static_cast<size_t>(place)]);
            }
        }
    }
    const bool canPick = m_ready != 0 && m_busy == 0 && current == 0 && hasMenu && s.menuSize("menu") > 0;
    if (event == input::kSelect || event == input::kSelect2) {
        if (canPick) {
            r.slot = static_cast<int32_t>(s.menuItemValue("menu", s.selectedMenuItem("menu")));
            if (event == input::kSelect) {
                r.next = 1;
            } else {
                r.remove = 1;
            }
            host.sound("CLICK");
        }
    } else if (event == input::kBack && m_busy == 0 && current == 0) {
        host.sound("CLICK");
        host.request("extras", SwitchMode::Replace);
        m_leaving = 1;
    }
}

}  // namespace oj::frontend
