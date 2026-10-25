// The race front end's save windows: savehighscores and savereplay.

#include "Game/Race/Hud/RaceSaves.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/JetTexts.h"

namespace oj {

using save::FlagKind;
using save::SaveFlag;

void SaveHighScoresWindow::again() {
    m_closed = 0;
    m_written = 0;
    m_again = 0;
    m_failed = 0;
    m_answer = -1;
    m_data->saveDevice.reset();
    m_frames = 0;
    m_current = 0;
    m_formatting = 99998;
    m_question = 99996;
    m_writeError = 99995;
    m_stateNote = 99994;
    m_data->saveRequest.request = save::Request::Write;
}

void SaveHighScoresWindow::enter(GameData& data, const std::filesystem::path& saveFolder) {
    *this = SaveHighScoresWindow{};
    m_open = true;
    m_data = &data;
    m_side.data = &data;
    m_side.saveFolder = saveFolder;
    m_data->saveRequest.clear();
    again();
}

void SaveHighScoresWindow::update(RaceSaveHost& host) {
    save::SaveRequest& r = m_data->saveRequest;
    if (m_closed != 0) return;
    if (m_written != 0) {
        host.closeMessage();
        m_closed = 1;
        m_open = false;
        m_data->saveDevice.reset();
        host.setSaveBusy(0);  // its exit (0x00465E20) and newhighscore_cms's (0x004664A0)
        log::info("race: the routes' records written");
        return;
    }
    if (m_again != 0) {
        again();
        return;
    }
    m_current = host.messageUp() ? host.lastMessageNumber() : 0;
    m_data->saveDevice.stepHighScores(r, m_side);
    if (m_failed != 0) {
        if (m_answer != -1) {
            if (m_answer == 1) {
                m_written = 1;
            } else {
                again();
            }
            return;
        }
        if (m_question != m_current && host.gameTexts() != nullptr) {
            host.raceMessage(ui::MessageType::YesNoModeless, host.gameTexts()->at(kTextHiScoresNotSaved), &m_answer,
                             true);
            m_question = host.lastMessageNumber();
        }
        return;
    }
    for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
        if (r.flags[k] == 0) continue;
        const FlagKind kind = save::kFlagKinds[k];
        const SaveFlag flag = static_cast<SaveFlag>(k);
        if (kind == FlagKind::Message) {
            switch (flag) {
            case SaveFlag::NoSpace:
                // 0x0046617D: the flag's line, line 4, the size, "KB", line 5, 1492.
                host.raceMessage(
                    ui::MessageType::Continue,
                    host.jetText(save::flagText(flag)) + host.jetText(kJetAtLeast) + std::to_wstring(r.neededKB) +
                        L"KB" + host.jetText(kJetAreRequired) +
                        (host.gameTexts() != nullptr ? host.gameTexts()->at(kTextToSaveHiScores) : std::wstring()),
                    nullptr, false);
                r.clear();
                r.next = 1;
                host.setSaveBusy(0);
                break;
            case SaveFlag::Saving:
                host.setSaveBusy(1);  // 0x00465FBE: the device busy
                break;
            case SaveFlag::Formatting:
                if (m_formatting != m_current) {
                    host.raceMessage(ui::MessageType::Note, host.jetText(save::flagText(flag)), nullptr, false);
                    m_formatting = host.lastMessageNumber();
                }
                host.setSaveBusy(1);
                break;
            case SaveFlag::Saved:
                m_written = 1;
                host.setSaveBusy(0);
                break;
            case SaveFlag::Formatted:
                if (m_formatting == m_current) host.closeMessage();
                host.setSaveBusy(0);
                break;
            case SaveFlag::WriteError:
                if (m_current != m_writeError) {
                    host.raceMessage(ui::MessageType::ContinueModeless, host.jetText(save::flagText(flag)), &r.next,
                                     false);
                    m_writeError = host.lastMessageNumber();
                    r.clear();
                }
                m_failed = 1;
                host.setSaveBusy(0);
                break;
            case SaveFlag::FormatError:
                if (m_formatting == m_current) {
                    host.raceMessage(ui::MessageType::Continue, host.jetText(save::flagText(flag)), nullptr, false);
                    m_formatting = host.lastMessageNumber();
                    again();
                }
                host.setSaveBusy(0);
                break;
            case SaveFlag::TooMany:
                host.raceMessage(ui::MessageType::Continue, L"loadsaveUI.cpp: THIS MSG SHOULD NEVER APPEAR!", nullptr,
                                 false);
                r.clear();
                r.next = 1;
                m_failed = 1;
                host.setSaveBusy(0);
                break;
            default: break;
            }
        } else if (kind == FlagKind::Question && (flag == SaveFlag::FormatAsk || flag == SaveFlag::Exists)) {
            // 0x0046628B / 0x00466309: the other questions' flags ask nothing here.
            if (m_question != m_current) {
                const std::wstring overwrite =
                    host.gameTexts() != nullptr ? host.gameTexts()->at(kTextOverwriteHiScores) : std::wstring();
                const std::wstring text = flag == SaveFlag::FormatAsk
                                              ? host.jetText(save::flagText(SaveFlag::Unformatted)) +
                                                    host.jetText(save::flagText(SaveFlag::FormatAsk))
                                              : overwrite;
                host.raceMessage(ui::MessageType::YesNoModeless, text, &r.answer, true);
                m_question = host.lastMessageNumber();
            }
            host.setSaveBusy(0);
        } else if (kind == FlagKind::State && flag != SaveFlag::Inserted && flag != SaveFlag::Unformatted &&
                   m_stateNote != m_current) {
            if (m_frames > 14) {
                host.raceMessage(ui::MessageType::ContinueModeless, host.jetText(save::flagText(flag)), nullptr, false);
                m_stateNote = host.lastMessageNumber();
                m_frames = 0;
                host.setSaveBusy(0);
            } else {
                ++m_frames;
            }
        }
    }
}

void SaveReplayWindow::again() {
    save::SaveRequest& r = m_data->saveRequest;
    r.clear();
    m_data->saveDevice.reset();
    r.request = m_listing == 0 ? save::Request::Write : save::Request::None;
    m_failed = 0;
    m_again = 0;
    m_closed = 0;
    m_done = 0;
    m_answer = -1;
    m_ended = -1;
    m_question = 99999;
    m_formatting = 99997;
    m_noSpace = 99995;
    m_writeError = 99994;
}

void SaveReplayWindow::enter(GameData& data, const std::filesystem::path& saveFolder) {
    *this = SaveReplayWindow{};
    m_open = true;
    m_data = &data;
    m_side.data = &data;
    m_side.saveFolder = saveFolder;
    again();
    m_pending = 99991;
    m_stateNote = 99996;
}

void SaveReplayWindow::update(RaceSaveHost& host) {
    save::SaveRequest& r = m_data->saveRequest;
    host.setSaveBusy(0);  // 0x00465412
    if (m_closed != 0) return;
    if (m_done != 0) {
        m_closed = 1;
        m_open = false;
        m_data->saveDevice.reset();
        host.setSaveBusy(0);  // its exit (0x004653D0)
        log::info("race: savereplay closed");
        return;
    }
    if (m_again != 0) {
        again();
        return;
    }
    m_current = host.messageUp() ? host.lastMessageNumber() : 0;
    const save::DeviceState state = m_data->saveDevice.stepReplay(r, m_side);
    if (m_failed != 0) {
        if (m_answer == -1) {
            if (host.gameTexts() != nullptr)
                // The stock's question names the PS2's memory card; the fixed build's "Save replay?" (435).
                host.raceMessage(ui::MessageType::YesNo, host.gameTexts()->at(kTextSaveReplay), &m_answer, false);
            m_answer = -1;
            return;
        }
        if (m_answer == 0) {
            m_done = 1;
        } else {
            again();
        }
        return;
    }
    if (m_ended != -1) {
        if (m_ended == 0) {
            m_done = 1;
            return;
        }
        m_ended = -1;
        m_listing = 0;
        again();
        return;
    }
    if (m_current == m_pending) return;
    if (state != save::DeviceState::Write && r.flag(SaveFlag::NoDevice) == 0) {
        m_failed = 1;
        return;
    }
    for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
        if (r.flags[k] == 0) continue;
        const FlagKind kind = save::kFlagKinds[k];
        const SaveFlag flag = static_cast<SaveFlag>(k);
        if (kind == FlagKind::Message) {
            switch (flag) {
            case SaveFlag::NoSpace:
                if (m_noSpace != m_current) {
                    const std::wstring toSave =
                        host.gameTexts() != nullptr ? host.gameTexts()->at(kTextToSaveReplay) : std::wstring();
                    host.raceMessage(ui::MessageType::ContinueModeless,
                                     host.jetText(save::flagText(flag)) + host.jetText(kJetAtLeast) +
                                         std::to_wstring(r.neededKB) + L"KB" + host.jetText(kJetAreRequired) + toSave,
                                     &r.next, false);
                    m_noSpace = host.lastMessageNumber();
                    r.clear();
                }
                host.setSaveBusy(0);
                break;
            case SaveFlag::Saving:
                host.setSaveBusy(1);  // 0x004655CE: the device busy
                break;
            case SaveFlag::Formatting:
                if (m_formatting != m_current) {
                    host.raceMessage(ui::MessageType::Note, host.jetText(save::flagText(flag)), nullptr, false);
                    m_formatting = host.lastMessageNumber();
                }
                host.setSaveBusy(1);
                break;
            case SaveFlag::Saved:
                host.raceMessage(ui::MessageType::Continue, host.jetText(save::flagText(flag)), nullptr, false);
                m_done = 1;
                host.setSaveBusy(0);
                break;
            case SaveFlag::Formatted:
                if (m_formatting == m_current) host.closeMessage();
                host.setSaveBusy(0);
                break;
            case SaveFlag::WriteError:
                if (m_current != m_writeError) {
                    host.raceMessage(ui::MessageType::Continue, host.jetText(save::flagText(flag)), &r.next, false);
                    m_writeError = host.lastMessageNumber();
                    r.clear();
                }
                host.setSaveBusy(0);
                break;
            case SaveFlag::FormatError:
                if (m_formatting == m_current) {
                    host.raceMessage(ui::MessageType::Continue, host.jetText(save::flagText(flag)), nullptr, false);
                    m_formatting = host.lastMessageNumber();
                    r.clear();
                    r.next = 1;
                    m_failed = 1;
                }
                host.setSaveBusy(0);
                break;
            case SaveFlag::TooMany:
                host.raceMessage(ui::MessageType::Continue, L"loadsaveUI.cpp: THIS MSG SHOULD NEVER APPEAR!", nullptr,
                                 false);
                r.clear();
                r.next = 1;
                m_failed = 1;
                host.setSaveBusy(0);
                break;
            default: break;
            }
        } else if (kind == FlagKind::Question) {
            if (flag != SaveFlag::FormatAsk) {
                r.clear();
                m_data->saveDevice.reset();
                r.request = save::Request::Write;
                host.setSaveBusy(0);
                return;
            }
            if (m_question != m_current) {
                host.raceMessage(ui::MessageType::YesNoModeless,
                                 host.jetText(save::flagText(SaveFlag::Unformatted)) +
                                     host.jetText(save::flagText(SaveFlag::FormatAsk)),
                                 &r.answer, true);
                m_question = host.lastMessageNumber();
                host.setSaveBusy(0);
            }
        } else if (kind == FlagKind::State && flag != SaveFlag::Inserted && flag != SaveFlag::Unformatted) {
            if (m_frames < 15) {
                ++m_frames;
            } else if (m_stateNote != m_current) {
                host.raceMessage(ui::MessageType::ContinueModeless, host.jetText(save::flagText(flag)), &m_failed,
                                 false);
                m_stateNote = host.lastMessageNumber();
                m_frames = 0;
                host.setSaveBusy(0);
            }
        }
    }
}

}  // namespace oj
