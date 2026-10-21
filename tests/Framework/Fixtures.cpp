#include "Framework/Fixtures.h"

#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Career/Career.h"
#include "Core/Saves/Save.h"

#include <cstdio>
#include <string_view>

namespace oj::test {

namespace {

std::filesystem::path g_saveRoot;
std::filesystem::path g_gameRoot;

std::string_view textOf(const std::vector<std::byte>& bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

void setSaveRoot(const std::filesystem::path& root) { g_saveRoot = root; }

const std::filesystem::path& saveRoot() { return g_saveRoot; }

const GameFiles& saveFiles() {
    static const GameFiles files(g_saveRoot);
    return files;
}

void setGameRoot(const std::filesystem::path& root) { g_gameRoot = root; }

const GameFiles& gameFiles() {
    static const GameFiles game = [] {
        GameFiles files(g_gameRoot);
        if (!files.valid()) {
            std::printf("     no game folder (--game): the game data checks are left out\n");
        }
        return files;
    }();
    return game;
}

InGameFolder::InGameFolder() {
    if (!gameFiles().valid()) return;
    m_before = std::filesystem::current_path();
    std::filesystem::current_path(gameFiles().root());
    m_valid = true;
}

InGameFolder::~InGameFolder() {
    if (m_valid) std::filesystem::current_path(m_before);
}

const SharedGameData& sharedGameData() {
    static const std::unique_ptr<SharedGameData> data = [] {
        auto made = std::make_unique<SharedGameData>();
        const GameFiles& game = gameFiles();
        made->game = game.valid();
        if (!made->game) return made;
        made->tables = ExeTables::stock();
        const auto text = game.read(std::filesystem::path("scripts") / "cardefs.txt");
        if (!text) return made;
        CrtRandom loadRandom;
        made->defs.load(textOf(*text), made->tables, loadRandom);
        for (const char* file : PartCatalogue::kFiles) {
            if (const auto part = game.read(std::filesystem::path("scripts") / file)) {
                made->catalogue.load(textOf(*part), made->tables);
            }
        }
        made->dynamics = std::make_unique<DynamicsStore>(game);
        made->carDefsRead = true;
        return made;
    }();
    return *data;
}

const CarDynamics* eclipseDynamics() {
    const SharedGameData& data = sharedGameData();
    if (!data.carDefsRead) return nullptr;
    const CarDef* eclipse = data.defs.byModel("ECLIPSE_STOCK");
    return eclipse != nullptr ? data.dynamics->of(*eclipse) : nullptr;
}

const std::vector<std::pair<std::string, Car>>& savedCars() {
    static const std::vector<std::pair<std::string, Car>> cars = [] {
        std::vector<std::pair<std::string, Car>> found;
        for (const save::Driver& d : save::listDrivers(g_saveRoot / L"SAVE")) {
            if (!d.hasArchive) continue;
            const std::string label = toUtf8(d.folder.filename().wstring());
            const GameFiles folder(d.folder);
            const auto file = folder.read(L"juicey.dat");
            if (!file) continue;
            save::Archive archive;
            if (!archive.read(*file, nullptr)) continue;
            const auto entry = archive.entries.find("CMSGlobal.sav");
            if (entry == archive.entries.end()) continue;
            auto career = std::make_unique<Career>();
            if (!career->read(entry->second, nullptr)) continue;
            for (const Car& car : career->garage) found.emplace_back(label, car);
        }
        return found;
    }();
    return cars;
}

}  // namespace oj::test
