// saveprofile: a new driver written through the save device.

#include "Game/Frontend/Saves/SaveProfile.h"

#include "Core/Base/Log.h"
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

// 0x00463DE0: the requests cleared, the device started again and asked to write (with the saving not turned on by
// this window), +0x0C -1, +0x14 .. +0x24 0.
void SaveProfile::again(SaveHost& host) {
    GameData* data = host.gameData();
    save::SaveRequest& r = data->saveRequest;
    r.clear();
    data->saveDevice.reset();
    r.request = m_turnedOn == 0 ? save::Request::Write : save::Request::None;
    m_ended = 0;
    m_left = 0;
    m_done = 0;
    m_writeAgain = 0;
    m_answer = -1;
}

// 0x00463E40: the shown messages' numbers out of reach, busy, "Please wait..." (jettext line 3, type 0), +0x10 -1,
// the career to be made (+0x68), the write asked. OpenJuice keeps the new driver (its name, settings, help pages) in
// the profile until here; the save system takes it now.
void SaveProfile::enter(SaveHost& host) {
    GameData* data = host.gameData();
    if (data == nullptr) return;
    *this = SaveProfile{};
    m_continueAsked = 99999;
    m_unusedNotes[0] = 99998;
    m_writingNote = 99997;
    m_deviceNote = 99996;
    m_waitNote = 99995;
    m_spaceNote = 99994;
    m_errorNote = 99993;
    m_ps2Asked = 99992;
    m_unusedNotes[1] = 99991;
    m_overwriteAsked = 99990;
    m_formatAsked = 99989;
    m_turnedOn = 0;
    m_closeTime = 0.0f;
    host.setSaveBusy(1);
    if (host.profile()) {
        data->profileName = host.profile()->name;
        data->saving = 1;  // 0x004675CB
        host.driverSettings() = host.profile()->settings;
        data->help = host.profile()->help;
    }
    host.message(MessageType::Note, host.jetText(kJetPleaseWait), false, nullptr);
    m_waitNote = host.lastMessageNumber();
    m_ps2Answer = -1;
    m_career = 1;
    again(host);
}

// 0x00463F00
void SaveProfile::leave(SaveHost& host) {
    GameData* data = host.gameData();
    if (data) data->saveDevice.reset();
    host.setSaveBusy(0);
}

// 0x00463F30. Done: menu (mode 1). Once "Please wait..." is in, the career head 0 and a new career (0x004A2300). The
// overwrite question answered no: the list left, the device again, the name window (NEW DRIVER NAME, "Player"),
// CLICK, keyboard. The device steps; a write that ended without its success (the device no longer writing, flag 2
// not set) asks "You will not be able to save your progress. Do you want to continue without saving? ..." (type 4,
// on NO): yes turns the saving off, "Autosave is disabled. ..." (type 1) and done; no writes again (and, with the
// saving turned on by the window, asks the PS2 question 1994: Stock problem, a console's text). The flags while
// writing (0x006ED9F0): 4 the space (type 2), 9 busy, 10 its note once, 12 written: the save system's driver not
// changed, the saving on, the volumes (settingsVolumes), "Save Successful" (type 1), done; 13 the note of 10 closed; 16
// "Save failed! Data write error" (type 2) once; 17 its note when 10's is up; 19 the stock's debug text, ended; 6 the
// format question, 7 and 20 their question (type 4, on NO, into the answer); 2 (not on the PC) after 15 frames its
// note (type 2). Other flags of kind 1 break the stock game (int3); nothing here.
void SaveProfile::update(SaveHost& host) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    if (m_left != 0) return;
    if (m_done != 0) {
        host.request("menu", SwitchMode::Replace);
        m_left = 1;
        data->saveDevice.reset();
        return;
    }
    if (m_writeAgain != 0) {
        again(host);
        return;
    }
    m_current = host.messageNumber();
    if (m_waitNote == m_current) {
        if (host.messageUp() && !host.messageTransitionsDone()) return;
        if (m_career != 0) {
            log::info("frontend: new career after {} rand() values", gameRandom().draws());
            if (data->career) data->career->head = 0;
            data->newCareer(gameRandom(), crewName8(data->profileName));
            log::info("frontend: the career took {} rand() values in all", gameRandom().draws());
            data->arcade.load(data->arcadeRaces, data->tables);
            m_career = 0;
        }
    }
    if (m_overwriteAsked == m_current && r.answer == 0) {
        r.done = 1;
        data->saveDevice.reset();
        host.setUpDriverName(texts.at(kTextPlayer));
        host.sound("CLICK");
        host.request("keyboard", SwitchMode::Replace);
        return;
    }
    const save::DeviceState state = data->saveDevice.stepProfile(r, host.saveSide());
    if (m_ended != 0) {
        if (m_answer == -1) {
            if (m_current != m_continueAsked) {
                host.message(MessageType::YesNoModeless,
                             host.jetText(kJetCannotSave) + L" " + host.jetText(kJetNoAutosave), false, &m_answer,
                             true);
                m_continueAsked = host.lastMessageNumber();
                host.setSaveBusy(0);
            }
            return;
        }
        if (m_answer != 1) {
            again(host);
            m_ps2Answer = -1;
            if (m_turnedOn != 0 && r.skipCardQuestion == 0) {
                host.message(MessageType::YesNoModeless, texts.at(kTextPs2Save), false, &m_ps2Answer, true);
                m_ps2Asked = host.lastMessageNumber();
            }
            return;
        }
        data->saving = 0;
        m_done = 1;
        host.message(MessageType::Continue, host.jetText(kJetAutosaveDisabled), false, nullptr);
        return;
    }
    if (m_ps2Answer == -1) {
        if (m_current == m_ps2Asked) return;
        if (state == save::DeviceState::Write || r.flag(SaveFlag::NoDevice) != 0) {
            for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
                if (r.flags[k] == 0) continue;
                const FlagKind kind = save::kFlagKinds[k];
                const SaveFlag flag = static_cast<SaveFlag>(k);
                if (kind == FlagKind::Message) {
                    switch (flag) {
                    case SaveFlag::NoSpace:
                        if (m_spaceNote != m_current) {
                            // 0x00464588: the flag's line, line 4, the size, "KB", line 5, 1490.
                            host.message(MessageType::ContinueModeless,
                                         host.jetText(save::flagText(flag)) + host.jetText(kJetAtLeast) +
                                             std::to_wstring(r.neededKB) + L"KB" + host.jetText(kJetAreRequired) +
                                             texts.at(kTextToSaveDriver),
                                         false, &r.next);
                            m_spaceNote = host.lastMessageNumber();
                            r.clear();
                        }
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::Saving: host.setSaveBusy(1); break;
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
                        host.settingsVolumes();
                        host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                        r.clear();
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
                            m_current = host.lastMessageNumber();
                            r.clear();
                            r.next = 1;
                        }
                        host.setSaveBusy(0);
                        break;
                    case SaveFlag::TooMany:
                        host.message(MessageType::Continue, kNeverText, false, nullptr);
                        r.clear();
                        r.next = 1;
                        host.setSaveBusy(0);
                        m_ended = 1;
                        break;
                    default:
                        break;
                    }
                } else if (kind == FlagKind::Question) {
                    if (flag == SaveFlag::FormatAsk) {
                        if (m_formatAsked != m_current) {
                            host.message(MessageType::YesNoModeless,
                                         host.jetText(save::flagText(SaveFlag::Unformatted)) +
                                             host.jetText(save::flagText(SaveFlag::FormatAsk)),
                                         false, &r.answer, true);
                            m_formatAsked = host.lastMessageNumber();
                            host.setSaveBusy(0);
                        }
                    } else {
                        if (m_overwriteAsked != m_current) {
                            host.message(MessageType::YesNoModeless, host.jetText(save::flagText(flag)), false,
                                         &r.answer, true);
                            m_overwriteAsked = host.lastMessageNumber();
                        }
                        host.setSaveBusy(0);
                    }
                } else if (kind == FlagKind::State && flag != SaveFlag::Inserted && flag != SaveFlag::Unformatted) {
                    if (m_frames < 15) {
                        ++m_frames;
                    } else {
                        if (m_deviceNote != m_current) {
                            host.message(MessageType::ContinueModeless, host.jetText(save::flagText(flag)), false,
                                         nullptr);
                            m_deviceNote = host.lastMessageNumber();
                            m_frames = 0;
                        }
                        host.setSaveBusy(0);
                    }
                }
            }
            return;
        }
    } else if (m_ps2Answer != 0) {
        m_ps2Answer = -1;
        m_turnedOn = 0;
        again(host);
        return;
    }
    m_ended = 1;
}

}  // namespace oj::frontend
