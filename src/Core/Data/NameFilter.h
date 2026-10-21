#pragma once
// The name check of the stock game (0x00418A70): scripts\profanities.txt and scripts\nonprofanities.txt, a word per
// line, compared in lower case. A name is refused when its letters and digits, in lower case, hold a word of the
// first list and none of the second.

#include <string>
#include <string_view>
#include <vector>

namespace oj {

class NameFilter {
public:
    // The two files' bytes (8-bit text, lines ending with CR LF).
    void load(std::string_view profanities, std::string_view exceptions);
    bool refuses(std::wstring_view name) const;

private:
    std::vector<std::string> m_words;
    std::vector<std::string> m_exceptions;
};

}  // namespace oj
