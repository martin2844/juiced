// The driver window (driver) of the save device: the drivers listed, one read, one deleted, a new one started.

#include "Game/Frontend/Saves/Driver.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

using save::FlagKind;
using save::SaveFlag;

namespace {

// The PC block of jettext: the line of flag 0.

}  // namespace

// 0x004678C0: TITLE "Profile Manager" (2017, its FontModifier started again); the list cleared and asked for
// (0x00467800); the elements whose name holds DATE hidden; the records to be read again (their +0x1120 0, the main
// menu's loadhighscores); +0x44, +0x4C 0, the shown messages' numbers out of reach, +0x30 [0x0075EE64].
void Driver::enter(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (host.jetTexts().size() == 0) {
        host.jetTexts().load(std::filesystem::path("scripts") / ("jettext" + std::string(1, host.language().controlsLetter) +
                                                           ".txt"));
    }
    s.setText("TITLE", texts.at(kTextProfileManager));
    clearList(host, s);
    s.hideContaining("DATE");
    if (data) data->highScores.loaded = 0;
    m_current = 0;
    m_count = 0;
    m_deleteAsked = 0x98967F;
    m_loading = 0x98967E;
    m_deleting = 0x98967D;
    m_checking = 0x98967C;
    m_stateNote = 0x98967B;
    m_note = 0;  // [0x0075EE64]: written by nothing of the PC's
}

// 0x00467800: the window's state cleared (+0x1C .. +0x2C, +0x50; +0x14, +0x18 1), the requests cleared, MENU and
// backmenu emptied but for "CREATE NEW DRIVER" (value -1) and its bar, the device asked for the list (0) and
// started again.
void Driver::clearList(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    m_loaded = 0;
    m_leaving = 0;
    m_ready = 0;
    m_busy = 0;
    m_wait = 0.0f;
    m_filled = 1;
    m_refill = 1;
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    r.clear();
    if (s.find("menu") >= 0) {
        s.clearMenu("menu");
        s.addCommandItem("menu", texts.at(kTextCreateDriver), -1.0f, " ");
        if (s.find("backmenu") >= 0) {
            s.clearMenu("backmenu");
            s.addMenuItem("backmenu", L"", 0.0f, "", "ui_pan5_ora");
        }
    }
    r.request = save::Request::List;
    data->saveDevice.reset();
}

// 0x00467620: MENU and backmenu again: "CREATE NEW DRIVER", then the drivers listed (their place the value; the
// command the first byte of the name's UTF-16, as the name's characters are passed); past 10 a note "Cannot display
// all the savegames! Displaying the first 10." (1626) and the rest left out; the first driver highlighted (item 1:
// 0x004677C4 passes the list's being non-empty, 1 or 0, to 0x005AF8A0), "CREATE NEW DRIVER" without one; +0x14 0.
void Driver::fillList(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (s.find("menu") < 0 || data == nullptr) return;
    const bool back = s.find("backmenu") >= 0;
    s.clearMenu("menu");
    if (back) s.clearMenu("backmenu");
    s.addCommandItem("menu", texts.at(kTextCreateDriver), -1.0f, " ");
    if (back) s.addMenuItem("backmenu", L"", 0.0f, "", "ui_pan5_ora");
    uint32_t n = 0;
    for (const save::SaveSlot& slot : data->saveDevice.slots()) {
        ++n;
        if (n > 10) {
            host.message(MessageType::Continue, texts.at(kTextTooManySaves) + L"10.", false, nullptr);
            break;
        }
        const std::string command(1, slot.name.empty() ? '\0' : static_cast<char>(slot.name[0] & 0xFF));
        s.addCommandItem("menu", slot.name, static_cast<float>(slot.index), command);
        if (back) s.addMenuItem("backmenu", L"", 0.0f, "", "ui_pan5_ora");
    }
    s.selectMenuItem("menu", data->saveDevice.slots().empty() ? 0 : 1);
    m_filled = 0;
}

// 0x004679B0. MENU and backmenu take events only with no message up; the DEL_ elements show while a driver (not
// "CREATE NEW DRIVER") is highlighted, SEL_TEXT "SELECT" (1560). While a message is up the window waits for its
// transitions; leaving it closes a message up. Each frame the profile device steps; idle with a driver read it goes
// to the menu (mode 2), idle else the list is asked again. The flags (0x006EDA50):
//   0, 8: busy, "next" 0; 4: "next"; 5 (none) and 14 (listed) after the wait: the list ready (14: filled once,
//   0x00467620, its size kept); 11 read: "Load Successful" (type 1, kept up over the menu), the window to leave,
//   "next", the saving on, the driver's music and volumes (settingsVolumes); 15, 18: their note, "next", the list
//   cleared and asked again, the saving off, +0x54 0.5; 20: "Are you sure you want to delete driver <name>?" (type 4,
//   on NO, into the answer) once; 21: MENU back to "CREATE NEW DRIVER", "Delete successful!" (1496), the list to be
//   filled again; 22: "Could not delete driver." (Stock problem: the close flag the "Deleting..." note had set
//   closes this one the next frame; the fixed build closes the note before it), MENU back, the list again, +0x54
//   0.5; 23: "Deleting..." (type 0) once, busy;
//   1 .. 3 (not on the PC): the list again, 2 a note (type 2) once.
// SELECT (the list ready, not busy, no message): a driver: its slot read ("next"), CLICK; "CREATE NEW DRIVER" with
// fewer than 10 drivers: the list left, the device started again, the name window (NEW DRIVER NAME, "Player"),
// CLICK, keyboard (mode 1), the career's head 0, CLICK again; 10 or more: "There are too many drivers! ..." (1625),
// CLICK. Event 7 on a driver (as SELECT's conditions): delete its slot, CLICK. BACK (not busy, no message): the
// network's end (none on the PC), CLICK, start (mode 1), leaving. Then backmenu takes MENU's highlight.
void Driver::update(SaveHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    host.setSaveBusy(0);
    m_current = host.messageNumber();
    const bool hasMenu = s.find("menu") >= 0;
    if (hasMenu) {
        s.setMenuActive("menu", m_current == 0);
        if (s.find("backmenu") >= 0) s.setMenuActive("backmenu", m_current == 0);
        if (s.selectedMenuItem("menu") == 0) {
            s.hideContaining("DEL_");
        } else {
            s.showContaining("DEL_", false);
        }
        if (s.find("SEL_TEXT") >= 0) s.setText("SEL_TEXT", texts.at(kTextSelect));
    }
    if (m_current == m_loading || m_current == m_deleting) host.setSaveBusy(1);
    if (host.messageUp() && !s.transitionsDone()) return;
    if (m_leaving != 0) {
        if (host.messageUp() && host.lastMessageNumber() != 0) host.closeMessage();
        return;
    }
    if (m_wait > 0.0f) m_wait = m_wait - host.frameTime();
    const save::DeviceState state = data->saveDevice.stepProfile(r, host.saveSide());
    if (state == save::DeviceState::Idle) {
        if (m_loaded != 0) {
            if (m_loading == m_current && host.messageUp()) host.closeMessage();
            host.setSaveBusy(0);
            host.request("menu", SwitchMode::ReplaceNow);
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
            case SaveFlag::Loading:
                host.setSaveBusy(1);
                r.next = 0;
                break;
            case SaveFlag::NoSpace: r.next = 1; break;
            case SaveFlag::NoData:
            case SaveFlag::Listed:
                if (m_wait <= 0.0f) {
                    if (m_checking == m_current && host.messageUp()) host.closeMessage();
                    m_ready = 1;
                    m_busy = 0;
                    if (flag == SaveFlag::Listed && m_refill != 0) {
                        fillList(host, s);
                        m_refill = 0;
                        m_count = static_cast<uint32_t>(data->saveDevice.slots().size());
                    }
                }
                break;
            case SaveFlag::Loaded:
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                m_loaded = 1;
                m_ready = 1;
                m_busy = 0;
                r.next = 1;
                host.setSaveBusy(0);
                data->saving = 1;
                break;
            case SaveFlag::ReadError:
            case SaveFlag::Corrupt:
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                host.setSaveBusy(0);
                m_ready = 1;
                m_loaded = 0;
                m_busy = 0;
                r.next = 1;
                clearList(host, s);
                data->saving = 0;
                m_afterFailure = 0.5f;
                break;
            case SaveFlag::Deleted:
                if (hasMenu) {
                    s.clearMenu("menu");
                    s.addCommandItem("menu", texts.at(kTextCreateDriver), -1.0f, " ");
                    s.selectMenuItem("menu", 0);
                }
                host.setSaveBusy(0);
                host.message(MessageType::Continue, texts.at(kTextDeleted), false, nullptr);
                m_busy = 0;
                m_refill = 1;
                break;
            case SaveFlag::DeleteError:
                if (m_current == m_deleting && host.messageUp()) host.closeMessage();
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                host.setSaveBusy(0);
                if (hasMenu) {
                    s.clearMenu("menu");
                    s.addCommandItem("menu", texts.at(kTextCreateDriver), -1.0f, " ");
                    s.selectMenuItem("menu", 0);
                }
                m_refill = 1;
                m_busy = 0;
                m_afterFailure = 0.5f;
                break;
            case SaveFlag::Deleting:
                if (m_current != m_deleting) {
                    host.message(MessageType::Note, host.jetText(save::flagText(flag)), false, nullptr);
                    m_deleting = host.lastMessageNumber();
                    m_busy = 1;
                }
                host.setSaveBusy(1);
                break;
            default:
                break;
            }
        } else if (kind == FlagKind::State) {
            if (flag == SaveFlag::Inserted) m_note = 0;
            if (m_refill == 0 && flag != SaveFlag::Inserted) {
                if (hasMenu) {
                    s.clearMenu("menu");
                    s.addCommandItem("menu", texts.at(kTextCreateDriver), -1.0f, " ");
                }
                m_refill = 1;
                m_ready = 1;
            }
            if (flag == SaveFlag::NoDevice || flag == SaveFlag::Unformatted) {
                if (m_loading == m_current) {
                    if (host.messageUp()) host.closeMessage();
                    host.setSaveBusy(0);
                }
                if (flag == SaveFlag::NoDevice && m_current != m_stateNote && m_note == 0) {
                    m_note = 1;
                    host.message(MessageType::ContinueModeless, host.jetText(save::flagText(SaveFlag::NoDevice)), false,
                                 nullptr);
                    m_stateNote = host.lastMessageNumber();
                }
                m_busy = 0;
                m_ready = 1;
            }
        } else if (kind == FlagKind::Question && flag == SaveFlag::DeleteAsk && m_current != m_deleteAsked && hasMenu) {
            const size_t item = s.selectedMenuItem("menu");
            host.message(MessageType::YesNoModeless,
                         host.jetText(save::flagText(flag)) + s.menuItemText("menu", item) + L"?", false, &r.answer,
                         true);
            m_deleteAsked = host.lastMessageNumber();
        }
    }
    const bool canPick = m_ready != 0 && m_busy == 0 && m_current == 0;
    if (event == input::kSelect) {
        if (canPick) {
            if (hasMenu && s.menuSize("menu") > 0) {
                const float value = s.menuItemValue("menu", s.selectedMenuItem("menu"));
                if (value >= 0.0f) {
                    r.slot = static_cast<int32_t>(value);
                    r.next = 1;
                } else if (m_count < 10) {
                    r.done = 1;
                    data->saveDevice.reset();
                    host.setUpDriverName(texts.at(kTextPlayer));
                    host.sound("CLICK");
                    host.request("keyboard", SwitchMode::Replace);
                    if (data->career) data->career->head = 0;
                } else {
                    host.message(MessageType::Continue, texts.at(kTextTooManyDrivers), false, nullptr);
                }
            }
            host.sound("CLICK");
        }
    } else if (event == input::kSelect3) {
        if (canPick && hasMenu && s.menuSize("menu") > 0) {
            const float value = s.menuItemValue("menu", s.selectedMenuItem("menu"));
            if (value >= 0.0f) {
                r.slot = static_cast<int32_t>(value);
                r.remove = 1;
                host.sound("CLICK");
            }
        }
    } else if (event == input::kBack && m_busy == 0 && m_current == 0) {
        // 0x0046833E: online, the network game ended (with the matchmaking only) and the online objects let go.
        if (net::onlineUser() != nullptr) {
            if (net::matchmaking() != nullptr) net::endNetGame();
            net::endOnline();
        }
        host.sound("CLICK");
        host.request("start", SwitchMode::Replace);
        m_leaving = 1;
    }
    if (hasMenu && s.find("BackMenu") >= 0) s.selectMenuItem("BackMenu", s.selectedMenuItem("menu"));
}

}  // namespace oj::frontend
