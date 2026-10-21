// loadhighscores: the records read through the records' device over the main menu.

#include "Game/Frontend/Saves/LoadHighScores.h"

#include "Core/Base/Log.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

using save::FlagKind;
using save::SaveFlag;

namespace {

// The PC block of jettext: the line of flag 0.

}  // namespace

// 0x00465A90: the windows' enter; +0x0C .. +0x20 0, the requests cleared and the records asked for (0), the records'
// device started again; +0x28 -1.
void LoadHighScores::enter(SaveHost& host) {
    GameData* data = host.gameData();
    *this = LoadHighScores{};
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    r.clear();
    r.request = save::Request::List;
    data->saveDevice.reset();
    m_wait = -1.0f;
}

// 0x00465E20 (loadhighscores' and savehighscores' exit): the records' device started again, not busy.
void LoadHighScores::leave(SaveHost& host) {
    GameData* data = host.gameData();
    if (data) data->saveDevice.reset();
    host.setSaveBusy(0);
}

// 0x00465B00. Not busy; +0x28 counts down. Read: once +0x28 is 0 or below the message up is closed and the window
// closes itself (mode 6). Else the records' device steps; reading, its flags (0x006ED9F0): 4 its note (type 1, the
// space), "next"; 8 busy; 11 the records read (their +0x1120 1), then as 15: "next", read; 18 "Unable to load
// hi-scores!" (type 1), "next", read; a flag of kind 0 (not 1) ends the reading. Not reading: read.
void LoadHighScores::update(SaveHost& host) {
    GameData* data = host.gameData();
    if (data == nullptr) return;
    save::SaveRequest& r = data->saveRequest;
    host.setSaveBusy(0);
    if (m_wait > 0.0f) m_wait = m_wait - host.frameTime();
    if (m_closing != 0) return;
    if (m_read != 0) {
        if (m_wait <= 0.0f) {
            if (host.messageUp()) host.closeMessage();
            host.request("loadhighscores", SwitchMode::Pop);
            m_closing = 1;
        }
        return;
    }
    m_current = host.messageNumber();
    const save::DeviceState state = data->saveDevice.stepHighScores(r, host.saveSide());
    if (state != save::DeviceState::Read) {
        m_read = 1;
        return;
    }
    for (size_t k = 0; k < save::kFlagKinds.size(); ++k) {
        if (r.flags[k] == 0) continue;
        const FlagKind kind = save::kFlagKinds[k];
        const SaveFlag flag = static_cast<SaveFlag>(k);
        if (kind == FlagKind::Message) {
            switch (flag) {
            case SaveFlag::NoSpace:
                host.message(MessageType::Continue, host.jetText(save::flagText(flag)), false, nullptr);
                r.clear();
                r.next = 1;
                break;
            case SaveFlag::Loading: host.setSaveBusy(1); break;
            case SaveFlag::Loaded:
                data->highScores.loaded = 1;
                log::info("frontend: the routes' records read");
                [[fallthrough]];
            case SaveFlag::ReadError:
                r.next = 1;
                m_read = 1;
                break;
            case SaveFlag::Corrupt:
                host.message(MessageType::Continue, host.jetText(kJetHiScoresNotLoaded), false, nullptr);
                r.next = 1;
                m_read = 1;
                break;
            default:
                break;
            }
        } else if (kind == FlagKind::State && flag != SaveFlag::Inserted) {
            m_read = 1;
            break;
        }
    }
}

}  // namespace oj::frontend
