#pragma once
// What the integration tests share, made once when first asked: the player's save folder, the installed game (the
// command line's --game folder), the built-in name tables, cardefs.txt, the parts catalogue, the cars' dynamics and the
// cars of the saved careers. Without a game folder the tests of the game's data return at once.

#include "Core/Base/GameFiles.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/ExeTables.h"
#include "Core/Physics/Dynamics.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace oj::test {

// The save folder (the command line's, else %APPDATA%\THQ\Juiced) and its files.
void setSaveRoot(const std::filesystem::path& root);
const std::filesystem::path& saveRoot();
const GameFiles& saveFiles();

// The installed game's folder (the command line's --game) and its files (not valid() without one).
void setGameRoot(const std::filesystem::path& root);
const GameFiles& gameFiles();

// The current directory the game's folder while it lives, as the game runs (the windows, the texts and the archives
// are read from there); not valid() without a game folder, the directory then left as it is.
class InGameFolder {
public:
    InGameFolder();
    ~InGameFolder();
    InGameFolder(const InGameFolder&) = delete;
    InGameFolder& operator=(const InGameFolder&) = delete;
    bool valid() const { return m_valid; }

private:
    std::filesystem::path m_before;
    bool m_valid = false;
};

// The game's data the tests read: the built-in tables, then with cardefs.txt read the car models (with a rand()
// of their own, as the start does), the ten files of the parts catalogue and the store of the cars' dynamics.
struct SharedGameData {
    bool game = false;  // the game folder was found
    ExeTables tables;
    bool carDefsRead = false;  // cardefs.txt read with the tables: defs, catalogue and dynamics made
    CarDefs defs;
    PartCatalogue catalogue;
    std::unique_ptr<DynamicsStore> dynamics;
};
const SharedGameData& sharedGameData();

// The dynamics of ECLIPSE_STOCK (dynamic_eclipse.txt); none without the game's data.
const CarDynamics* eclipseDynamics();

// The cars in the garages of the careers saved in the save folder, with the folder's name.
const std::vector<std::pair<std::string, Car>>& savedCars();

}  // namespace oj::test
