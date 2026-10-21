#pragma once
// Log of the engine: UTF-8 text file next to the executable, every line also to the debugger output.

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace oj::log {

enum class Level { Info, Warning, Error };

// Opens (truncates) the log file. Lines written before it go to the debugger output only.
void open(const std::filesystem::path& file);
void close();
void write(Level level, std::string_view text);
// The text to the debugger output alone, then a line's end (the stock's 0x00596D00: OutputDebugStringA of the text
// and of "\r\n").
void debugger(std::string_view text);

template <class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void warning(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Warning, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

// The stock game's debug print (0x0059AF00): vsprintf into the game's buffer ([0x0079E9D8]), then the debugger's
// output (OutputDebugStringA) only; not the log file.
void debugPrint(const char* format, ...);

}  // namespace oj::log
