// opts_voice: the voice chat's devices (docs/spec/28-network.md).

#include "Game/Frontend/Network/VoiceOptions.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/VoiceChat.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/Frontend/WindowTools.h"

#include <cstring>

namespace oj::frontend {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// A device name of the settings (0x40 bytes, up to a 0).
std::string settingsName(const save::Settings& set, uint32_t at) {
    std::string name;
    for (uint32_t i = 0; i < 0x40 && set.object[at + i] != 0; ++i) name.push_back(static_cast<char>(set.object[at + i]));
    return name;
}

// strncpy(settings + at, name, 0x40), the last byte 0.
void setSettingsName(save::Settings& set, uint32_t at, const std::string& name) {
    for (uint32_t i = 0; i < 0x40; ++i) set.object[at + i] = i < name.size() ? static_cast<uint8_t>(name[i]) : 0;
    set.object[at + 0x3F] = 0;
}

// 0x004841B0 / 0x00484200: the first device of the name (case aside), -1 without one.
int32_t findDevice(const std::vector<std::string>& devices, const std::string& name) {
    for (size_t i = 0; i < devices.size(); ++i) {
        if (_stricmp(devices[i].c_str(), name.c_str()) == 0) return static_cast<int32_t>(i);
    }
    return -1;
}

// The voice object's +0x04 (0x00565D70: 0x0057F190 into its array of ten, capture devices for 0, playback devices
// for 1), +0x08 (the count) and +0x0C (a name, 0x0057F0B0's cut description) as 0x004842B0 asks them: each name
// copied into 0x40 bytes, the last 0.
std::vector<std::string> deviceNames(net::VoiceChat& voice, int32_t playback) {
    std::vector<std::string> names;
    voice.listDevices(playback);
    for (uint32_t i = 0; i < voice.deviceCount(); ++i) {
        char name[0x40]{};
        const char* from = voice.deviceName(i);
        for (size_t k = 0; k < sizeof(name) && from[k] != 0; ++k) name[k] = from[k];
        name[sizeof(name) - 1] = 0;
        names.emplace_back(name);
    }
    return names;
}

// 0x0049A850: each byte widened with its sign. Stock problem: a byte from 0x80 up (a description in the ANSI code
// page) becomes 0xFF80 .. 0xFFFF, not its character.
std::wstring widenSigned(const std::string& text) {
    std::wstring wide;
    for (char c : text) wide.push_back(static_cast<wchar_t>(static_cast<int16_t>(static_cast<int8_t>(c))));
    return wide;
}

std::wstring wide(std::u16string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

// 0x00484700 (after the windows' enter): with a voice object already ([0x0075F044]) +0x0C 0; else the voice object
// made (0x004EECA0 to release any, 0x00565360: GameSpy Voice started, CoInitialize and the Speex coder, no device
// opened; its +0x1C 0x004EEB50 for the chat's on/off, voiceChatHook) and +0x0C 1. Both lists emptied (0x00485030), +0x38 0, +0x30
// and +0x34 -1; the lists made (0x004842B0); +0x30 and +0x34 the devices of the settings' names (+0x2D4, +0x314; -1
// for "none" or a name not listed); the texts (0x00484250).
void VoiceOptions::enter(OptionsHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (net::gameVoice() != nullptr) {
        m_madeVoice = 0;
    } else {
        net::makeGameVoice(voiceChatHook(data));
        m_madeVoice = 1;
    }
    m_inputs.clear();
    m_outputs.clear();
    m_relist = 0.0f;
    m_input = -1;
    m_output = -1;
    makeLists(host, s);
    const save::Settings& set = host.optionSettings();
    const std::string in = settingsName(set, save::Settings::kVoiceInput);
    const std::string out = settingsName(set, save::Settings::kVoiceOutput);
    m_input = _stricmp(in.c_str(), "none") == 0 ? -1 : findDevice(m_inputs, in);
    m_output = _stricmp(out.c_str(), "none") == 0 ? -1 : findDevice(m_outputs, out);
    showTexts(host, s);
    log::info("frontend: voice devices: {} in, {} out", m_inputs.size(), m_outputs.size());
}

// 0x00484830: +0x38 grows by the game's frame time; past 2 s it goes back to 0 and the lists are made again. SELECT
// on DON_BUT: the settings take the names chosen ("none" for -1; 0x40 bytes, the last 0), CLICK and the options'
// menu (mode 7; from a race's pause ([G+0x3C]) with the voice object and a local player of the user object
// ([0x0075F040]), the voice object's +0x10 opens the settings' devices for the first one first). LEFT on INPUT_BUT: with one chosen CLICK and the one before (the
// first: none); on OUTPUT_BUT the same; RIGHT: from none the first when there is one, else the next up to the last,
// with CLICK; both then write the texts whatever the button. BACK: CLICK and the options' menu, the settings left.
void VoiceOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    m_relist = m_relist + host.gameFrameTime();
    if (m_relist > 2.0f) {  // [0x006FF898]
        m_relist = 0.0f;
        makeLists(host, s);
    }
    const std::string button = s.currentButton();
    if (event == input::kSelect) {
        if (!same(button, "DON_BUT")) return;
        save::Settings& set = host.optionSettings();
        setSettingsName(set, save::Settings::kVoiceInput,
                        m_input == -1 ? "none" : m_inputs[static_cast<size_t>(m_input)]);
        setSettingsName(set, save::Settings::kVoiceOutput,
                        m_output == -1 ? "none" : m_outputs[static_cast<size_t>(m_output)]);
        host.sound("CLICK");
        if (host.racePause()) {
            net::VoiceChat* voice = net::gameVoice();
            net::OnlineUser* user = net::onlineUser();
            if (voice != nullptr && user != nullptr && !user->localPlayers.empty()) {
                const std::string in = settingsName(set, save::Settings::kVoiceInput);
                const std::string out = settingsName(set, save::Settings::kVoiceOutput);
                voice->openDevices(in.c_str(), out.c_str(), user->localPlayers[0]);
            }
        }
        host.request(host.optionsMenu(), SwitchMode::Swap);
    } else if (event == input::kLeft) {
        const auto back = [&](int32_t& chosen) {
            if (chosen == -1) return;
            host.sound("CLICK");
            chosen = chosen == 0 ? -1 : chosen - 1;
        };
        if (same(button, "INPUT_BUT")) {
            back(m_input);
        } else if (same(button, "OUTPUT_BUT")) {
            back(m_output);
        }
        showTexts(host, s);
    } else if (event == input::kRight) {
        const auto on = [&](int32_t& chosen, const std::vector<std::string>& devices) {
            if (chosen == -1) {
                if (devices.empty()) return;
                host.sound("CLICK");
                chosen = 0;
            } else if (chosen < static_cast<int32_t>(devices.size()) - 1) {
                host.sound("CLICK");
                ++chosen;
            }
        };
        if (same(button, "INPUT_BUT")) {
            on(m_input, m_inputs);
        } else if (same(button, "OUTPUT_BUT")) {
            on(m_output, m_outputs);
        }
        showTexts(host, s);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
    }
}

// 0x004847F0: the windows' leave (no Menu here), then the voice object released (0x004EECA0) when the enter made it.
void VoiceOptions::leave() {
    if (m_madeVoice != 0) net::endGameVoice();
}

// 0x004842B0: the names chosen kept ("none" for -1); with the voice object the capture devices listed again into
// +0x10 and the playback ones into +0x20; +0x30 and +0x34 the kept names' devices again (-1 for "none" or a device
// gone); INPUT and OUTPUT written as 0x00484250 does.
void VoiceOptions::makeLists(OptionsHost& host, ui::Screen& s) {
    const std::string in = m_input == -1 ? "none" : m_inputs[static_cast<size_t>(m_input)];
    const std::string out = m_output == -1 ? "none" : m_outputs[static_cast<size_t>(m_output)];
    if (net::VoiceChat* voice = net::gameVoice()) {
        m_inputs = deviceNames(*voice, 0);
        m_outputs = deviceNames(*voice, 1);
    }
    m_input = _stricmp(in.c_str(), "none") == 0 ? -1 : findDevice(m_inputs, in);
    m_output = _stricmp(out.c_str(), "none") == 0 ? -1 : findDevice(m_outputs, out);
    showTexts(host, s);
}

// 0x00484250: INPUT "None" (164) without a device, else its name (0x005AA1F0 through 0x0049A850); OUTPUT the same.
void VoiceOptions::showTexts(OptionsHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    const auto text = [&](const char* element, int32_t chosen, const std::vector<std::string>& devices) {
        if (s.find(element) < 0) return;
        s.setText(element, chosen == -1 ? texts.at(kTextNone)
                                        : widenSigned(devices[static_cast<size_t>(chosen)]));
    };
    text("INPUT", m_input, m_inputs);
    text("OUTPUT", m_output, m_outputs);
}

}  // namespace oj::frontend
