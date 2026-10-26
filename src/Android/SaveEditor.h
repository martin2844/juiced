#pragma once
#include "Core/Data/GameData.h"
#include "Core/Saves/Save.h"
#include <array>
namespace oj::saveedit {
struct Changes {
    int money;
    std::array<int, 8> respect;
    std::vector<std::string> addCars;
};
save::Archive load(GameData& game, const std::vector<std::byte>& bytes);
std::vector<uint8_t> edit(GameData& game, const std::vector<std::byte>& bytes, const Changes& changes);
std::string describe(GameData& game, const std::vector<std::byte>& bytes);
std::string quote(const std::string& text);
}
