#include "Core/Base/Script.h"

#include <cctype>
#include <cstdlib>

namespace oj {

namespace {

bool space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

}  // namespace

const std::string& ScriptLine::empty() {
    static const std::string kEmpty;
    return kEmpty;
}

int ScriptLine::integer(size_t i, int fallback) const {
    return i < words.size() ? static_cast<int>(std::strtol(words[i].c_str(), nullptr, 10)) : fallback;
}

float ScriptLine::real(size_t i, float fallback) const {
    return i < words.size() ? static_cast<float>(std::atof(words[i].c_str())) : fallback;
}

std::vector<ScriptLine> parseScript(std::string_view source) {
    // " ~" and the rest of its line, with the line break after it, become spaces (0x004FB7C0).
    std::string text(source);
    for (size_t at = text.find(" ~"); at != std::string::npos; at = text.find(" ~", at)) {
        size_t i = at;
        while (i < text.size() && text[i] != '\n' && text[i] != '\r') {
            text[i++] = ' ';
        }
        while (i < text.size() && (text[i] == '\n' || text[i] == '\r')) {
            text[i++] = ' ';
        }
    }

    std::vector<ScriptLine> lines;
    size_t start = 0;
    size_t number = 0;
    while (start < text.size()) {
        // Lines end at CR, LF or CR LF.
        size_t end = start;
        while (end < text.size() && text[end] != '\n' && text[end] != '\r') {
            ++end;
        }
        ++number;
        const std::string_view line(text.data() + start, end - start);
        start = end;
        if (start < text.size() && text[start] == '\r' && start + 1 < text.size() && text[start + 1] == '\n') {
            start += 2;
        } else if (start < text.size()) {
            start += 1;
        }

        size_t i = 0;
        while (i < line.size() && space(line[i])) ++i;
        if (i == line.size() || line[i] == ';') {
            continue;
        }
        ScriptLine out;
        out.number = number;
        const size_t keywordStart = i;
        while (i < line.size() && !space(line[i])) ++i;
        out.keyword = std::string(line.substr(keywordStart, i - keywordStart));
        // Words (0x004FBC40): white space separated, or from a quote to the next quote.
        while (true) {
            while (i < line.size() && space(line[i])) ++i;
            if (i == line.size()) {
                break;
            }
            if (line[i] == '"') {
                const size_t close = line.find('"', i + 1);
                const size_t stop = close == std::string_view::npos ? line.size() : close;
                out.words.emplace_back(line.substr(i + 1, stop - i - 1));
                i = close == std::string_view::npos ? line.size() : close + 1;
            } else {
                const size_t wordStart = i;
                while (i < line.size() && !space(line[i])) ++i;
                out.words.emplace_back(line.substr(wordStart, i - wordStart));
            }
        }
        lines.push_back(std::move(out));
    }
    return lines;
}

}  // namespace oj
