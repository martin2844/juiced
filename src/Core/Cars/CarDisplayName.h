#pragma once
#include "Core/Base/Text.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"

namespace oj {
inline std::wstring carDisplayName(const TextTable& texts, const CarDef& car) {
    // The retail text block ends at model 52. Later lines are unrelated UI text.
    if (car.id >= 1 && car.id <= 52) return texts.at(kTextModels + static_cast<size_t>(car.id));
    return toWide(car.displayName.empty() ? car.name : car.displayName);
}
} // namespace oj
