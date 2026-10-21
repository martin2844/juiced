#pragma once
// The text scripts of the stock game (docs/spec/05-scripts.md): lines of a keyword and its words. A line ending in
// " ~" goes on in the next one; lines starting with ';' are comments. The stock reader looks each keyword up in its
// handler class's map (0x004FB6C0 makes one from a table, vtable 0x006F6F34; 0x004FBEE0 / 0x004FBFC0 insert, case
// aside; 0x004FB770 / 0x004FBEA0 free; twenty made by the static initializers, 0x00767D28 ..); the port's readers
// take the keywords of each file in their own code.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

struct ScriptLine {
    std::string keyword;
    std::vector<std::string> words;  // white space separated, or "quoted" (without the quotes)
    size_t number = 0;               // of the line in the file, from 1

    // The word at i, or the fallback when the line has fewer words.
    const std::string& word(size_t i, const std::string& fallback = empty()) const {
        return i < words.size() ? words[i] : fallback;
    }
    int integer(size_t i, int fallback = 0) const;  // atol
    float real(size_t i, float fallback = 0.0f) const;  // atof

private:
    static const std::string& empty();
};

std::vector<ScriptLine> parseScript(std::string_view text);

}  // namespace oj
