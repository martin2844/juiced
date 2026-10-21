#include "Core/Data/NameFilter.h"

namespace oj {

namespace {

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::vector<std::string> lines(std::string_view text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\r', start);
        if (end == std::string_view::npos) end = text.size();
        std::string line(text.substr(start, end - start));
        for (char& c : line) c = lower(c);
        out.push_back(std::move(line));
        start = end + 2;  // CR LF
    }
    return out;
}

}  // namespace

void NameFilter::load(std::string_view profanities, std::string_view exceptions) {
    m_words = lines(profanities);
    m_exceptions = lines(exceptions);
}

bool NameFilter::refuses(std::wstring_view name) const {
    // The name's letters and digits (by the low byte of each character), in lower case.
    std::string key;
    for (wchar_t w : name) {
        const char c = static_cast<char>(w);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            key.push_back(lower(c));
        }
    }
    for (const std::string& word : m_words) {
        if (key.find(word) == std::string::npos) {
            continue;
        }
        bool excepted = false;
        for (const std::string& allowed : m_exceptions) {
            if (key.find(allowed) != std::string::npos) {
                excepted = true;
            }
        }
        if (!excepted) {
            return true;
        }
    }
    return false;
}

}  // namespace oj
