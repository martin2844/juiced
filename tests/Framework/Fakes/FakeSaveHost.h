#pragma once
// The save device's windows' front end of the tests (Game/Frontend/Saves/SaveHost.h) over FakeWindowHost: no driver
// name refused, the game's side of the device holding nothing, the volumes and the network's call doing nothing.

#include "Framework/Fakes/FakeWindowHost.h"

#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/Frontend/Saves/SaveHost.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oj::test {

class FakeSaveHost : public FakeWindowHost, public frontend::SaveHost {
public:
    bool nameRefused(const std::wstring&) const override { return false; }
    std::optional<save::Profile>& profile() override { return m_profile; }
    void setUpDriverName(const std::wstring&) override {}
    frontend::KeyboardSetup& keyboardSetup() override { return m_setup; }
    void setSaveBusy(int32_t) override {}
    TextTable& jetTexts() override { return m_jet; }
    save::SaveGameSide& saveSide() override { return m_side; }
    void settingsVolumes() override {}
    void netCallFrame() override {}
    void cancelNetCall() override {}

private:
    struct Side : save::SaveGameSide {
        void buildProfile(save::SaveSource&) override {}
        bool applyProfile(const save::SaveSlot&, const save::SaveSource&) override { return false; }
        void buildHighScores(save::SaveSource&) override {}
        bool applyHighScores(const save::SaveSource&) override { return false; }
        void writeRegCf1() override {}
        void readRegCf1() override {}
    } m_side;
    std::optional<save::Profile> m_profile;
    frontend::KeyboardSetup m_setup;
    TextTable m_jet;
};

}  // namespace oj::test
