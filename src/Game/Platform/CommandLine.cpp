#include "Game/Platform/CommandLine.h"

#include "Core/Data/Controls.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace oj {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// Words split at white space; a word opened by a quote runs to the closing quote.
std::vector<std::string> splitWords(const char* text) {
    std::vector<std::string> words;
    const char* p = text;
    while (*p != '\0') {
        while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        const bool quoted = *p == '"';
        if (quoted) {
            ++p;
        }
        const char* start = p;
        while (*p != '\0' && (quoted ? *p != '"' : !std::isspace(static_cast<unsigned char>(*p)))) {
            ++p;
        }
        words.emplace_back(start, p);
        if (*p != '\0') {
            ++p;
        }
    }
    return words;
}

// The main function's copies (0x0041877E ..): each byte sign-extended (movsx), at most `limit` characters, then the
// character at limit - 1 made 0 (the buffer's last): limit - 1 characters kept.
std::wstring widen(const std::string& text, size_t limit) {
    std::wstring out;
    for (char c : text) {
        if (out.size() == limit) {
            break;
        }
        out.push_back(static_cast<wchar_t>(static_cast<uint16_t>(static_cast<int16_t>(static_cast<signed char>(c)))));
    }
    if (out.size() >= limit) out.resize(limit - 1);
    return out;
}

void applyWinMainSwitch(const std::string& name, LaunchOptions& o) {
    static const struct {
        const char* name;
        float value;
    } kScales[] = {{"1", 2.0f}, {"2", 4.0f}, {"3", 6.0f}, {"4", 8.0f}, {"5", 32.0f}, {"6", 64.0f}, {"7", 100.0f}};
    for (const auto& s : kScales) {
        if (same(name, s.name)) {
            o.scale = s.value;
            return;
        }
    }
    if (same(name, "localfx")) {
        o.localEffects = true;
    } else if (same(name, "debugfx")) {
        o.localEffects = true;
        o.debugEffects = true;
    } else if (same(name, "nonpured3d")) {
        o.pureDevice = false;
    } else if (same(name, "mtd3d")) {
        o.multithreadedD3D = true;
    } else if (same(name, "rf")) {
        o.rf = true;
    } else if (same(name, "r")) {
        o.presentIntervalOne = true;
    } else if (same(name, "x")) {
        o.audioFlagsOr |= 8;
    } else if (same(name, "s")) {
        o.softwareVertexProcessing = true;
    } else if (same(name, "n")) {
        o.audioFlagsOne = true;
    }
}

}  // namespace

LaunchOptions parseCommandLine(const char* commandLine) {
    LaunchOptions o;
    o.arguments = splitWords(commandLine != nullptr ? commandLine : "");
    const auto& args = o.arguments;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
#ifdef _DEBUG
        if (a.rfind("--", 0) == 0) {
            if (a == "--frames" && i + 1 < args.size()) {
                o.frames = std::strtoull(args[i + 1].c_str(), nullptr, 10);
            } else if (a == "--screen" && i + 1 < args.size()) {
                o.screen = args[i + 1];
            } else if (a == "--press" && i + 1 < args.size()) {
                size_t start = 0;
                const std::string& list = args[i + 1];
                while (start < list.size()) {
                    size_t end = list.find(',', start);
                    if (end == std::string::npos) end = list.size();
                    const std::string item = list.substr(start, end - start);
                    const size_t colon = item.find(':');
                    if (colon != std::string::npos) {
                        const std::string key = item.substr(colon + 1);
                        const auto named = keyboardCode("DIKEYBOARD_" + key);
                        const auto code = named ? *named : static_cast<uint8_t>(std::strtoul(key.c_str(), nullptr, 0));
                        const std::string frames = item.substr(0, colon);
                        const size_t dash = frames.find('-');
                        LaunchOptions::Press p;
                        p.down = std::strtoull(frames.substr(0, dash).c_str(), nullptr, 10);
                        p.up = dash == std::string::npos ? p.down + 1
                                                         : std::strtoull(frames.substr(dash + 1).c_str(), nullptr, 10);
                        p.code = code;
                        o.presses.push_back(p);
                    }
                    start = end + 1;
                }
            } else if (a == "--seed" && i + 1 < args.size()) {
                o.seed = static_cast<uint32_t>(std::strtoul(args[i + 1].c_str(), nullptr, 10));
            } else if (a == "--saves" && i + 1 < args.size()) {
                o.saves = args[i + 1];
            } else if (a == "--capture" && i + 1 < args.size()) {
                o.capture = args[i + 1];
            } else if (a == "--test") {
                o.test = true;
            } else if (a == "--players" && i + 1 < args.size()) {
                o.players = std::atoi(args[i + 1].c_str());
            } else if (a == "--track" && i + 1 < args.size()) {
                o.track = args[i + 1];
            } else if (a == "--car" && i + 1 < args.size()) {
                o.car = args[i + 1];
            } else if (a == "--track-camera" && i + 1 < args.size()) {
                o.trackCamera = args[i + 1];
            } else if (a == "--net" && i + 1 < args.size()) {
                o.net = args[i + 1];
            } else if (a == "--autopilot") {
                o.autopilot = true;
            }
            continue;
        }
#endif
        if (a.size() > 1 && a[0] == '-') {
            applyWinMainSwitch(a.substr(1), o);
        }
        const bool hasValue = i + 1 < args.size();
        if (same(a, "-i")) {
            o.instantRace = true;
        } else if (same(a, "-wet")) {
            o.wet = true;
        } else if (same(a, "-m")) {
            o.m = false;
        } else if (same(a, "-cruise")) {
            o.cruise = true;
        } else if (same(a, "-automod")) {
            o.automod = true;
            if (hasValue) {
                o.automodValue = std::atol(args[i + 1].c_str());
                // The main function's loop passes the value over, whatever it is; WinMain reads every word itself.
                const std::string& v = args[i + 1];
                if (v.size() > 1 && v[0] == '-') applyWinMainSwitch(v.substr(1), o);
                ++i;
            }
        } else if (_strnicmp(a.c_str(), "-host", 5) == 0) {
            o.host = true;
            o.connect = false;
        } else if (same(a, "-connect") && hasValue) {
            o.connectAddress = args[i + 1].substr(0, 31);
            o.connect = true;
            o.host = false;
        } else if (same(a, "-name") && hasValue) {
            o.playerName = widen(args[i + 1], 31);
        } else if (same(a, "-email") && hasValue) {
            o.email = widen(args[i + 1], 51);
        } else if (same(a, "-ppass") && hasValue) {
            o.password = widen(args[i + 1], 31);
        } else if (same(a, "-setup")) {
            o.setup = true;
        }
    }
    return o;
}

}  // namespace oj
