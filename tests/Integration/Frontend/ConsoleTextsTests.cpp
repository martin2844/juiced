// Integration tests: the fixed build's texts in place of the consoles' (docs/fixes.md, texts and console leftovers):
// autosave's question when it turns the saving on (Frontend/Saves/Autosave) and the note of the second player's
// lost controller (Frontend/Controllers.cpp), with the game's windows and texts.

#include "Framework/Test.h"
#include "Framework/Fakes/FakeSaveHost.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Fixtures.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"
#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/Saves/Autosave.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using oj::test::check;

// A folder beside the tests, made empty.
std::filesystem::path testFolder(const wchar_t* name) {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path folder = std::filesystem::path(exe).parent_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(folder, ec);
    std::filesystem::create_directories(folder, ec);
    return folder;
}

}  // namespace

// Autosave with the saving off (the player went on without saving once) turns it on and asks before it writes: the
// fixed build asks "Are you sure you want to save?" (1504), not the stock's question about the PS2's memory card in
// MEMORY CARD slot 1 (1994).
OJ_INTEGRATION_TEST(ConsoleTexts, AutosaveAsksToSave) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::GameData data(oj::test::gameFiles().root());
    data.tables = oj::ExeTables::stock();
    data.saveDevice.setFolder(testFolder(L"ConsoleTextsTest") / L"SAVE");
    data.saving = 0;
    oj::test::FakeSaveHost host;
    host.data = &data;
    const auto s = host.open("autosave");
    if (!s) return;
    oj::frontend::Autosave window;
    window.enter(host, *s);
    for (int frame = 0; frame < 10 && host.messages.empty(); ++frame) window.update(host, *s);
    check(!host.messages.empty(), "a question asked");
    if (host.messages.empty()) return;
    const std::wstring& asked = host.messages.front().text;
    check(asked == host.texts.at(oj::kTextSaveAsk) && asked != host.texts.at(oj::kTextPs2Save) &&
              host.messages.front().type == oj::ui::MessageType::YesNoModeless,
          "the question: \"Are you sure you want to save?\" (1504), yes or no");
}

// The second player's controller lost: the note (msgbox_critical) has no text, as the first player's; the stock's has
// "To continue, please reconnect the DUALSHOCK 2 analog controller into controller port 2." (1632). The note's text as
// OpenJuice.log has it.
OJ_INTEGRATION_TEST(ConsoleTexts, SecondControllerNoteEmpty) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    const oj::Language& language = *oj::Language::byName("english");
    oj::TextTable texts;
    texts.load(std::filesystem::path("scripts") / "english.txt");
    const std::filesystem::path logFile = testFolder(L"ConsoleTextsLog") / L"OpenJuice.log";
    {
        oj::Frontend front(device, language, texts, nullptr, logFile.parent_path());
        front.playerUi(1);
        oj::log::open(logFile);
        front.watchSecondController(false, 1, true);
        oj::log::close();
    }
    std::ifstream in(logFile, std::ios::binary);
    const std::string log((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    check(log.find("note of a lost controller \"\"") != std::string::npos && log.find("DUALSHOCK") == std::string::npos,
          "the second player's note opened with no text");
}
