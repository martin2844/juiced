#include "Core/Saves/Profile.h"

#include <iterator>

namespace oj::save {

uint64_t newProfileId(CrtRandom& random) {
    const auto r1 = static_cast<uint32_t>(random.next());
    const auto r2 = static_cast<uint32_t>(random.next());
    const auto r3 = static_cast<uint32_t>(random.next());
    const auto r4 = static_cast<uint32_t>(random.next());
    const uint32_t high = ((r2 ^ (r1 << 16)) & 0xFFFF0000u) | ((r3 ^ (r2 << 16)) >> 16);
    const uint32_t low = r4 ^ (r3 << 16);
    return (static_cast<uint64_t>(high) << 32) | low;
}

Profile Profile::create(std::wstring name, int languageIndex, CrtRandom& random) {
    Profile p;
    p.name = std::move(name);
    p.id = newProfileId(random);
    p.settings = Settings::defaults(languageIndex);
    p.unsaved = true;
    p.help.shown.assign(std::size(UiHelp::kPages), 0);
    return p;
}

}  // namespace oj::save
