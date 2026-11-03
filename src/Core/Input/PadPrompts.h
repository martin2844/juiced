#pragma once
// Not stock: the keyboard's hint pictures drawn as the pad's buttons for the same UI actions while a game controller
// was the last device used (ui_selecta Enter: A, ui_back Esc: B, ui_selectb PgDn: Y, ui_selectc Tab: X).

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace oj::input {

void usedPad(bool pad);
// The pad's picture to draw for a hint picture while the pad is in use; nullptr otherwise.
const char* promptTexture(const std::string& name);
// A pad picture's .ddt; none for other names.
std::optional<std::vector<std::byte>> promptBytes(const std::string& name);

}  // namespace oj::input
