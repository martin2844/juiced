#pragma once
// A front end for the tests of the windows: what a window asks of WindowHost is kept for the test to read (the
// switches asked, the messages, the sounds), what it reads the test sets (the game's data, the texts, the language,
// the frame time). A family's host derives from it as Frontend implements them all:
//
//     class FakeMainMenuHost : public FakeWindowHost, public frontend::MainMenuHost { ... };
//
// The windows' screens and the texts are read from the game's folder (Framework/Fixtures.h: InGameFolder): open.

#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Test.h"
#include "Game/Frontend/WindowHost.h"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// A family's fake takes these methods from FakeWindowHost through the virtual WindowHost (by dominance, C4250): that
// is the point of it.
#pragma warning(disable : 4250)

namespace oj::test {

class FakeWindowHost : public virtual frontend::WindowHost {
public:
    struct Message {
        ui::MessageType type = ui::MessageType::Note;
        std::wstring text;
        int* answer = nullptr;
        bool startNo = false;
    };

    // What the windows asked.
    std::vector<std::pair<std::string, frontend::SwitchMode>> requests;  // (state, mode)
    std::vector<Message> messages;
    std::vector<std::string> sounds;
    int closedMessages = 0;

    // What the windows read.
    GameData* data = nullptr;
    TextTable texts;
    Language lang = *Language::byName("english");
    ui::Screen* base = nullptr;
    float dt = 0.05f;
    uint32_t scroll = 0;
    frontend::SharedState sharedState;
    std::optional<save::Settings> settings;
    NullRenderDevice nullDevice;

    // A switch asked that the front end has not made yet; a test makes it by clearing this.
    bool switchPending = false;
    bool messageOpen = false;

    bool request(const std::string& state, frontend::SwitchMode mode) override {
        requests.emplace_back(state, mode);
        switchPending = true;
        return true;
    }
    bool switchAsked() const override { return switchPending; }
    ui::Screen& baseScreen() override { return *base; }
    void message(ui::MessageType type, const std::wstring& text, bool, int* answer, bool startNo) override {
        messages.push_back({type, text, answer, startNo});
        messageOpen = true;
        ++m_messageCount;
    }
    void messageTitle(const std::wstring&) override {}
    bool messageUp() const override { return messageOpen; }
    void closeMessage() override {
        messageOpen = false;
        ++closedMessages;
    }
    int32_t messageNumber() const override { return messageOpen ? m_messageCount : 0; }
    int32_t lastMessageNumber() const override { return m_messageCount; }
    bool messageTransitionsDone() const override { return true; }
    const std::wstring& jetText(size_t) const override { return m_empty; }
    void setCrewNameTexts(const std::wstring&) override {}
    void sound(const char* name) override { sounds.emplace_back(name); }
    void sound(const char* name, float, uint32_t) override { sounds.emplace_back(name); }

    GameData* gameData() override { return data; }
    const TextTable& gameTexts() const override { return texts; }
    const Language& language() const override { return lang; }
    scene::Engine* engine() override { return nullptr; }
    CrtRandom& viewLoading() override { return m_random; }
    std::optional<save::Settings>& driverSettings() override { return settings; }
    render::IRenderDevice& device() override { return nullDevice; }
    float frameTime() override { return dt; }
    uint32_t scrollHeld() const override { return scroll; }
    float gameFrameTime() const override { return dt; }

    // As the front end: the driver's, else a new driver's.
    uint32_t settingsValue(save::Settings::Field field) override {
        return settings ? settings->u32(field) : save::Settings::defaults(lang.index).u32(field);
    }
    save::Settings& optionSettings() override {
        if (!settings) settings = save::Settings::defaults(lang.index);
        return *settings;
    }
    float settingsMusic() override { return std::bit_cast<float>(settingsValue(save::Settings::kMusicVolume)); }
    void setMusicVolume(float, bool) override {}
    void startMusic() override {}
    UiSound& uiSounds() override { return m_uiSounds; }

    void baseEnter(ui::Screen&, const std::string&) override {}
    void baseLeave(ui::Screen&, const std::string&) override {}
    void helpPage(const std::string&, bool) override {}
    bool overlayUp() const override { return false; }
    void closeOverlay() override {}
    frontend::SharedState& shared() override { return sharedState; }

    // A window of the current directory (the game's folder) as the front end opens it, the language's texts read
    // first, made the base window; none, and a FAIL, when the game has no such window. It refers to the host: it goes
    // before it.
    std::shared_ptr<ui::Screen> open(const std::string& window) {
        if (texts.size() == 0) texts.load(std::filesystem::path("scripts") / (std::string(lang.name) + ".txt"));
        auto s = std::make_shared<ui::Screen>(nullDevice, lang);
        if (!s->load(window)) {
            check(false, window + ".ui read from the game's folder");
            return nullptr;
        }
        base = s.get();
        return s;
    }

    // The states asked for, in order.
    std::vector<std::string> states() const {
        std::vector<std::string> out;
        for (const auto& [state, mode] : requests) out.push_back(state);
        return out;
    }

private:
    int32_t m_messageCount = 0;
    std::wstring m_empty;
    CrtRandom m_random;
    UiSound m_uiSounds;
};

}  // namespace oj::test
