#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
#include "Mods/Creative.h"
#endif
// The front end's side of the save device (docs/spec/03-saves.md, "The save device"): what a driver's and the
// records' files hold, and the texts of the device's messages.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/WindowTools.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"

#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/NetSession.h"

#include "Core/Base/GameFiles.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Log.h"
#include "Core/Saves/OnlineAccount.h"
#include "Core/Saves/Save.h"
#include "Core/Base/Text.h"

#include <format>
#include <fstream>

namespace oj {

namespace {

constexpr char kNameCharacters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

}  // namespace

// The name of a new driver: DONE makes the driver (0x004675E0) and goes on to the crew logo, BACK returns to the
// driver list.
void Frontend::setUpDriverName(const std::wstring& text) {
    frontend::KeyboardSetup& k = m_keyboard.setup();
    k.title = m_gameTexts.at(kTextNewDriverName);
    k.text = text;
    k.minLength = 1;
    k.maxLength = 15;
    k.small = k.maxLength > 26;
    k.checkName = true;
    k.accept = [this](const std::wstring& name) {
        m_profile = save::Profile::create(name, m_language.index, gameRandom());
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
        mods::profileName(toUtf8(name));mods::requestPortrait();
#endif
        log::info("frontend: new driver {}", toUtf8(name));
        return true;
    };
    k.allowed = kNameCharacters;
    k.back = "driver";
    k.next = "cmschooselogo";
}

const std::wstring& Frontend::jetText(size_t line) const {
    static const std::wstring none;
    return line < m_jetTexts.size() ? m_jetTexts.at(line) : none;
}

// 0x004FA190: USERDATA (1.38, the driver's name) and juicey.dat of the save system's entries (0x0040A610:
// settings.dat, ArcadeUnlock.sav, UIHelp.sav, CMSGlobal.sav) in the driver's folder ("P" and the name, 0x0040AA20).
// The entries of the driver's juicey.dat that OpenJuice does not make (a mod's) are kept.
void Frontend::buildProfile(save::SaveSource& source) {
    if (m_data == nullptr) return;
    source.folder = save::driverFolderName(m_data->profileName);
    save::UserData userData;
    userData.value = 1.38f;
    userData.name = m_data->profileName;
    source.files["USERDATA"] = userData.write();
    save::Archive archive;
    if (const auto file = GameFiles(m_saveFolder / L"SAVE" / source.folder).read(L"juicey.dat")) {
        std::string error;
        if (!archive.read(*file, &error)) archive.entries.clear();
    }
    // The fixed build: the GameSpy account protected in its own entry (Core/Saves/OnlineAccount), settings.dat's
    // fields of it written empty (the stock's plain text); an account Windows does not protect is not kept.
    save::Settings settings = optionSettings();
    const save::OnlineAccount account = save::OnlineAccount::of(settings);
    archive.entries.erase(save::kOnlineAccountEntry);
    if (!account.empty()) {
        if (std::vector<uint8_t> entry = save::protectAccount(account); !entry.empty()) {
            archive.entries[save::kOnlineAccountEntry] = std::move(entry);
        }
        save::OnlineAccount{}.into(settings);
    }
    archive.entries["settings.dat"] = settings.write();
    archive.entries["ArcadeUnlock.sav"] = m_data->arcade.progress().write();
    archive.entries["UIHelp.sav"] = m_data->help.write();
    if (m_data->career) archive.entries["CMSGlobal.sav"] = m_data->career->write();
    source.files["juicey.dat"] = archive.write();
}

// 0x004FA2B0: the save system takes the driver's name (+0x20), folder (+0x38) and the entries of its juicey.dat
// (+0x08, 0x0040A830); a juicey.dat whose layout or hash does not hold is not taken.
bool Frontend::applyProfile(const save::SaveSlot& slot, const save::SaveSource& source) {
    const auto file = source.files.find("juicey.dat");
    save::Archive archive;
    std::string error;
    if (file == source.files.end()) {
        log::error("frontend: driver {} not loaded: no juicey.dat", toUtf8(slot.name));
        return false;
    }
    std::vector<std::byte> bytes(file->second.size());
    std::memcpy(bytes.data(), file->second.data(), bytes.size());
    if (!archive.read(bytes, &error)) {
        log::error("frontend: driver {} not loaded: {}", toUtf8(slot.name), error);
        return false;
    }
    std::string names;
    for (const auto& [name, data] : archive.entries) names += " " + name;
    log::info("frontend: driver {} loaded:{}", toUtf8(slot.name), names);
    if (m_data) m_data->profileName = slot.name;
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
    mods::profileName(toUtf8(slot.name));mods::requestPortrait();
#endif
    driverSettings().reset();
    if (const auto found = archive.entries.find("settings.dat"); found != archive.entries.end()) {
        save::Settings settings;
        // settings.dat (0x00409C90): the driver's settings, the crew's name into the game's texts.
        if (settings.read(found->second)) {
            // The fixed build: the protected account over settings.dat's fields (a stock save's plain account is
            // taken as it is and protected at the next save; one that does not decrypt, another Windows user's, is
            // none).
            if (const auto entry = archive.entries.find(save::kOnlineAccountEntry); entry != archive.entries.end()) {
                save::unprotectAccount(entry->second).value_or(save::OnlineAccount{}).into(settings);
            }
            setCrewNameTexts(widenSigned(settings.crewName()));
            driverSettings() = settings;
        }
    }
    // UIHelp.sav: the help pages shown (0x00485240).
    if (const auto found = archive.entries.find("UIHelp.sav"); m_data && found != archive.entries.end()) {
        m_data->help.read(found->second);
    }
    // ArcadeUnlock.sav: the arcade progress (0x0042ACA0; 0x0042ABF0 writes it).
    if (const auto found = archive.entries.find("ArcadeUnlock.sav"); m_data && found != archive.entries.end()) {
        save::ArcadeUnlock progress;
        if (progress.read(found->second)) m_data->arcade.read(progress);
    }
    if (m_data) m_data->applyModUnlocks();
    // CMSGlobal.sav (0x0040A9EF): the career's head, then 0x004BD790: a new career over the career objects
    // (0x004A2300, with the driver's name the save system has just taken, and its rand() draws) and the saved parts
    // read over it (the cars find their CarDefs by the model names); the phone's message slots (+0x28 each) cleared
    // again (0x004B6070).
    if (const auto found = archive.entries.find("CMSGlobal.sav"); m_data && found != archive.entries.end()) {
        m_data->newCareer(gameRandom(), crewName8(m_data->profileName));
        std::string why;
        if (m_data->career->read(found->second, &why)) {
            for (Car& car : m_data->career->garage) {
                car.def = findCarDef(m_data->defs, car.modelName());
                // Android editor .5 omitted the dealer's rating calculation. Recover
                // those cars on load without altering their parts, damage or progress.
                if (car.rating == 0 && car.def && m_data->dynamics)
                    rateCar(car, m_data->dynamics->of(*car.def), m_data->catalogue);
            }
        } else {
            log::error("frontend: the career of {} is not read: {}", toUtf8(slot.name), why);
        }
    }
    // The driver's settings choose the music again once the driver is in (0x00408D20 from 0x00467D8B).
    if (m_data && driverSettings()) m_data->shuffleMusic(*driverSettings(), gameRandom());
    // Then (0x004679B0, flag 11) the settings' volumes: the music's, the effects' and the speech's.
    settingsVolumes();
    return true;
}

// 0x004FAA30 / 0x0040A370: the records' USERDATA in SAVE\HS.
void Frontend::buildHighScores(save::SaveSource& source) {
    if (m_data == nullptr) return;
    source.folder = L"HS";
    source.files["USERDATA"] = m_data->highScores.write();
}

// 0x004FAAC0 / 0x0040A440
bool Frontend::applyHighScores(const save::SaveSource& source) {
    const auto file = source.files.find("USERDATA");
    return m_data != nullptr && file != source.files.end() && m_data->highScores.read(file->second);
}

// 0x005A33A0: reg.cf1 of the graphics options next to SAVE, the save root made first (0x00481630).
void Frontend::writeRegCf1() {
    const std::vector<uint8_t> reg = graphicsRegCf1(graphicsConfig()).write();
    save::makeFolders(m_saveFolder);
    std::ofstream(m_saveFolder / L"reg.cf1", std::ios::binary)
        .write(reinterpret_cast<const char*>(reg.data()), static_cast<std::streamsize>(reg.size()));
}

// 0x005A3410. Stock problem: the detail is read but the renderer's factor ([0x0079E670]) keeps the start's (60 %)
// until the graphics options are left; the fixed build makes the factor from the detail read.
void Frontend::readRegCf1() {
    if (const auto reg = GameFiles(m_saveFolder).read(L"reg.cf1")) {
        save::RegCf1 graphics;
        if (graphics.read(*reg)) {
            GraphicsConfig& c = graphicsConfig();
            graphicsFromRegCf1(c, graphics);
            c.lodBias = detailFactor(c.detail);
            setDetailFactor(c.lodBias);
        }
    }
}

// The network's delayed call autosave runs (0x00464A10): made when its time is out, else its time run down.
void Frontend::netCallFrame() {
    if (m_netCall.on == 0) return;
    if (m_netCall.left <= 0.0f) {
        if (m_netCall.call != nullptr) (this->*m_netCall.call)();
        m_netCall.on = 0;
    } else {
        m_netCall.left = m_netCall.left - m_frameTime;
    }
}

}  // namespace oj
