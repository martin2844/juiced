// autosave: the driver written through the save device over a window.

#include "Game/Frontend/Saves/Autosave.h"

#include "Core/Data/GameTexts.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

using save::FlagKind;
using save::SaveFlag;

namespace {

// The PC block of jettext: the line of flag 0.
// 0x006F24F8: the stock game's text of a flag its PC device never sets.
constexpr const wchar_t* kNeverText = L"loadsaveUI.cpp: THIS MSG SHOULD NEVER APPEAR!";

}  // namespace

// 0x00464830: as 0x00463DE0 for autosave, +0x10 -1, the shown messages' numbers out of reach, +0x60 1.
void Autosave::again(SaveHost& host) {
    GameData* data = host.gameData();
    save::SaveRequest& r = data->saveRequest;
    r.clear();
    data->saveDevice.reset();
    r.request = m_turnedOn == 0 ? save::Request::Write : save::Request::None;
    m_ended = 0;
    m_left = 0;
    m_done = 0;
    m_writeAgain = 0;
    m_ps2Answer = -1;
    m_questionAsked = 99999;
    m_unusedNotes[0] = 99998;
    m_writingNote = 99997;
    m_unusedNotes[1] = 99995;
    m_unusedNotes[2] = 99991;
    m_closeTime = 1.0f;
}

// 0x004648C0: the saving turned on when it is off (+0x28 1, +0x0C 0); the elements holding PROGRESS hidden; asked by
// the driver ([0x0075ED38]) TEXT "SAVING..." (1611, its FontModifier started again); +0x2C, +0x30 1.
void Autosave::enter(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    *this = Autosave{};
    m_answer = -1;
    if (data->saving == 0) {
        data->saving = 1;
        m_turnedOn = 1;
        m_answer = 0;
    }
    again(host);
    s.hideContaining("PROGRESS");
    if (host.shared().autosaveAsked != 0 && s.find("TEXT") >= 0) s.setText("TEXT", texts.at(kTextSavingAuto));
    m_deviceNote = 99996;
    m_errorNote = 99994;
    m_ps2Asked = 99993;
    m_unusedNotes[3] = 0;
    m_checkStateFlag = 1;
    m_checkFreeSpace = 1;
}

// 0x004649D0: the device started again, not busy, the network's call cancelled, [0x0075ED38] 0.
void Autosave::leave(SaveHost& host) {
    GameData* data = host.gameData();
    if (data) data->saveDevice.reset();
    host.setSaveBusy(0);
    host.cancelNetCall();
    host.shared().autosaveAsked = 0;
}

// 0x00464A10. Not busy; +0x60 counts down. Done, at 0 or below: the window closes itself (mode 6). Else the network's
// delayed call ([0x0075ED3C] .. [0x0075ED44], set only by CMSOnlineGridWalk, 0x004D40B0): its time at 0 or below it is
// made and turned off, else its time runs down by the frame time; then the device steps. A first step not writing: flag 3 or [0x00768084] under [0x00768080] (once each) write again; without
// flag 2 the end (flag 4 turns the window's saving back). The end: "You will not be able to save your progress. Do
// you want to continue without saving? ..." (type 4, on NO): yes turns the saving off, "Autosave is disabled. ..."
// (type 1), done; no writes again and, with the saving turned on by the window, asks the PS2 question 1994 (Stock
// problem: a console's text on the PC) whose yes writes again. The flags while writing: 4 the space (type 1), 9 TEXT
// shown, busy; 10 its note once, busy; 12 written: the driver not changed, the saving on, the requests cleared, asked
// by the driver "Save Successful" (type 1), TEXT hidden, +0x60 -1, done; 13 the note of 10 closed; 16 "Save failed!
// Data write error" (type 2) once; 17 its note when 10's is up and write again; 19 the stock's debug text; 6 the
// format question; 7: a driver not changed is written over without asking (the answer 1), else the question (type 4,
// on NO) once; 20 its question; 2 (not on the PC) its note after 15 frames.
void Autosave::update(SaveHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    host.setSaveBusy(0);
    if (m_left != 0) return;
    if (m_closeTime > 0.0f) m_closeTime = m_closeTime - host.frameTime();
    if (m_done != 0) {
        if (m_closeTime <= 0.0f) {
            host.request("autosave", SwitchMode::Pop);
            m_left = 1;
        }
        return;
    }
    if (m_writeAgain != 0) {
        again(host);
        return;
    }
    host.netCallFrame();
    const save::DeviceState state = data->saveDevice.stepProfile(r, host.saveSide());
    m_current = host.messageNumber();
    if (m_ended == 0) {
        if (m_ps2Answer == -1) {
            if (m_current == m_ps2Asked) return;
            if (state != save::DeviceState::Write) {
                if (r.flag(SaveFlag::Unformatted) != 0 && m_checkStateFlag != 0) {
                    m_ps2Answer = -1;
                    m_turnedOn = 0;
                    again(host);
                    m_checkStateFlag = 0;
                    m_answer = -1;
                    return;
                }
                if (r.freeKB < r.neededKB && m_checkFreeSpace != 0) {
                    m_ps2Answer = -1;
                    m_turnedOn = 0;
                    again(host);
                    m_checkFreeSpace = 0;
                    m_answer = -1;
                } else if (r.flag(SaveFlag::NoDevice) == 0) {
                    if (r.flag(SaveFlag::NoSpace) != 0) m_turnedOn = 0;
                    m_ended = 1;
                    return;
                }
            }
            for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
                if (r.flags[k] == 0) continue;
                const FlagKind kind = save::kFlagKinds[k];
                const SaveFlag flag = static_cast<SaveFlag>(k);
                if (kind == FlagKind::Message) {
                    switch (flag) {
                    case SaveFlag::NoSpace:
                        // 0x0046505D: the flag's line, "KB", line 5, 1490 (Stock problem: line 4 and the size
                        // are left out).
                        host.message(MessageType::Continue,
                                     host.jetText(save::flagText(flag)) + L"KB" + host.jetText(kJetAreRequired) +
                                         texts.at(kTextToSaveDriver),
                                     false, nullptr);
                        r.clear();
                        r.next = 1;
                        m_answer = -1;
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::Saving:
                        if (s.find("TEXT") >= 0) s.setHidden("TEXT", false);
                        host.setSaveBusy(1);
                        break;
                    case SaveFlag::Formatting:
                        if (m_writingNote != m_current) {
                            host.message(MessageType::Note, host.jetText(save::flagText(flag)), false, nullptr);
                            m_writingNote = host.lastMessageNumber();
                        }
                        host.setSaveBusy(1);
                        break;
                    case SaveFlag::Saved:
                        data->driverChanged = 0;
                        data->saving = 1;
                        r.clear();
                        if (host.shared().autosaveAsked != 0)
                            host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                        if (s.find("TEXT") >= 0) s.setHidden("TEXT", true);
                        m_closeTime = -1.0f;
                        m_done = 1;
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::Formatted:
                        if (m_writingNote == m_current && host.messageUp()) host.closeMessage();
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::WriteError:
                        if (m_current != m_errorNote) {
                            host.message(MessageType::ContinueModeless, host.jetText(save::flagText(flag)), false,
                                         &r.next);
                            m_errorNote = host.lastMessageNumber();
                            r.clear();
                        }
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::FormatError:
                        if (m_writingNote == m_current) {
                            host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                            m_writingNote = host.lastMessageNumber();
                            r.clear();
                            r.next = 1;
                            again(host);
                        }
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::TooMany:
                        host.message(MessageType::Continue, kNeverText, false, nullptr);
                        r.clear();
                        r.next = 1;
                        host.setSaveBusy(0);
                        m_answer = -1;
                        m_ended = 1;
                        break;
                    default:
                        break;
                    }
                } else if (kind == FlagKind::Question) {
                    if (flag == SaveFlag::FormatAsk) {
                        m_answer = -1;
                        m_checkStateFlag = 0;
                        if (m_questionAsked != m_current) {
                            host.message(MessageType::YesNoModeless,
                                         host.jetText(save::flagText(SaveFlag::Unformatted)) +
                                             host.jetText(save::flagText(SaveFlag::FormatAsk)),
                                         false, &r.answer, true);
                            m_questionAsked = host.lastMessageNumber();
                            host.setSaveBusy(0);
                        }
                    } else {
                        if (flag == SaveFlag::Exists && data->driverChanged == 0) {
                            r.answer = 1;
                            continue;
                        }
                        if (m_questionAsked != m_current) {
                            host.message(MessageType::YesNoModeless, host.jetText(save::flagText(flag)), false,
                                         &r.answer, true);
                            m_questionAsked = host.lastMessageNumber();
                            host.setSaveBusy(0);
                        }
                    }
                } else if (kind == FlagKind::State && flag != SaveFlag::Inserted && flag != SaveFlag::Unformatted) {
                    if (m_frames < 15) {
                        ++m_frames;
                    } else if (m_deviceNote != m_current) {
                        host.message(MessageType::ContinueModeless, host.jetText(save::flagText(flag)), false, nullptr);
                        m_deviceNote = host.lastMessageNumber();
                        m_frames = 0;
                        host.setSaveBusy(0);
                        m_answer = -1;
                    }
                }
            }
        } else {
            if (m_ps2Answer != 0) {
                m_ps2Answer = -1;
                m_turnedOn = 0;
                again(host);
                return;
            }
            m_ended = 1;
            if (m_answer == 0) {
                m_answer = -1;
                return;
            }
        }
        return;
    }
    if (m_answer == -1) {
        if (m_current == m_questionAsked) return;
        host.message(MessageType::YesNoModeless, host.jetText(kJetCannotSave) + L" " + host.jetText(kJetNoAutosave),
                     false, &m_answer, true);
        m_questionAsked = host.lastMessageNumber();
        m_answer = -1;
    } else if (m_answer == 1) {
        data->saving = 0;
        m_done = 1;
        host.message(MessageType::Continue, host.jetText(kJetAutosaveDisabled), false, nullptr);
    } else {
        again(host);
        if (m_turnedOn == 0 || r.skipCardQuestion != 0) return;
        host.message(MessageType::YesNoModeless, texts.at(kTextPs2Save), false, &m_ps2Answer, true);
        m_ps2Asked = host.lastMessageNumber();
        m_answer = -1;
    }
}

}  // namespace oj::frontend
