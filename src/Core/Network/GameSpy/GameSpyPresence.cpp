#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyPresenceDetail.h"

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Base/Random.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>

namespace oj::net::gp {

namespace detail {

constexpr std::string_view kFinal = "\\final\\";
constexpr std::string_view kSpaces = "                                                ";  // 48

// The narrow strings of the wide calls (gpConnectW, gpConnectNewUserW, gpInvitePlayerW): each character's low byte.
std::string narrow(std::u16string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char16_t c : text) out.push_back(static_cast<char>(c));
    return out;
}

// 0x00568E10: each byte zero-extended.
std::u16string widenBytes(std::string_view text) {
    std::u16string out;
    for (char c : text) out.push_back(static_cast<char16_t>(static_cast<uint8_t>(c)));
    return out;
}

// strncpy into size bytes with the last made 0: at most size - 1 characters.
std::string cut(std::string_view text, size_t size) {
    return std::string(text.substr(0, std::min(text.size(), size - 1)));
}

// The C string of the bytes: up to the first 0.
std::string_view cString(const std::string& text) {
    return std::string_view(text.c_str());
}

// gpiValueForKey (0x00579AF0): the key anywhere in the message (strstr), its value up to the next character equal to
// the key's first (the delimiter) or the end, at most size - 1 characters; false without the key.
bool valueForKey(std::string_view message, std::string_view key, std::string& out, size_t size) {
    const size_t at = message.find(key);
    if (at == std::string_view::npos) return false;
    out.clear();
    const char delimiter = key.front();
    for (size_t i = at + key.size(); i < message.size() && out.size() + 1 < size; ++i) {
        const char c = message[i];
        if (c == '\0' || c == delimiter) break;
        out.push_back(c);
    }
    return true;
}

long atolOf(const std::string& text) {
    return std::atol(text.c_str());
}

std::string slashes(std::string text) {
    for (char& c : text) {
        if (c == '\\') c = '/';
    }
    return text;
}

std::u16string fromUtf8Cut(std::string_view text, size_t size) {
    std::u16string out = fromUtf8(text);
    if (size != 0 && out.size() > size - 1) out.resize(size - 1);
    return out;
}

}  // namespace detail

namespace {

using namespace detail;

// The SDK's generator of the password encoding ([0x0071B1CC]).
uint32_t g_passSeed = 0;

uint32_t passRandom() {
    const uint32_t hi = (g_passSeed >> 16) * 16807u;
    uint32_t lo = (g_passSeed & 0xFFFF) * 16807u + ((hi & 0x7FFF) << 16);
    if (lo > 0x7FFFFFFF) lo = (lo & 0x7FFFFFFF) + 1;
    g_passSeed = lo + (hi >> 15);
    if (g_passSeed > 0x7FFFFFFF) g_passSeed = (g_passSeed & 0x7FFFFFFF) + 1;
    return g_passSeed;
}

// 0x0056E7D0: base64 by groups of three bytes (a short last group filled with zeros, 0x0056E740), the characters past
// the bytes the padding; mode 1 '[' ']' '_'.
std::string base64(const uint8_t* bytes, size_t size) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]";
    std::string out;
    for (size_t i = 0; i < size; i += 3) {
        uint8_t group[3] = {};
        for (size_t j = 0; j < 3 && i + j < size; ++j) group[j] = bytes[i + j];
        out.push_back(kAlphabet[group[0] >> 2]);
        out.push_back(kAlphabet[((group[0] & 3) << 4) | (group[1] >> 4)]);
        out.push_back(kAlphabet[((group[1] & 0xF) << 2) | (group[2] >> 6)]);
        out.push_back(kAlphabet[group[2] & 0x3F]);
    }
    if (size % 3 == 1) {
        out[out.size() - 2] = '_';
        out[out.size() - 1] = '_';
    } else if (size % 3 == 2) {
        out[out.size() - 1] = '_';
    }
    return out;
}

}  // namespace

void Md5Context::block(const uint8_t* p) {
    static const uint32_t kK[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static const int kS[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                               5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                               4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                               6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    auto rotate = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    uint32_t m[16];
    for (int i = 0; i < 16; ++i) {
        m[i] = static_cast<uint32_t>(p[i * 4]) | (static_cast<uint32_t>(p[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(p[i * 4 + 2]) << 16) | (static_cast<uint32_t>(p[i * 4 + 3]) << 24);
    }
    uint32_t aa = m_state[0], bb = m_state[1], cc = m_state[2], dd = m_state[3];
    for (int i = 0; i < 64; ++i) {
        uint32_t f = 0;
        int g = 0;
        if (i < 16) {
            f = (bb & cc) | (~bb & dd);
            g = i;
        } else if (i < 32) {
            f = (dd & bb) | (~dd & cc);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = bb ^ cc ^ dd;
            g = (3 * i + 5) % 16;
        } else {
            f = cc ^ (bb | ~dd);
            g = (7 * i) % 16;
        }
        const uint32_t t = dd;
        dd = cc;
        cc = bb;
        bb = bb + rotate(aa + f + kK[i] + m[g], kS[i]);
        aa = t;
    }
    m_state[0] += aa;
    m_state[1] += bb;
    m_state[2] += cc;
    m_state[3] += dd;
}

void Md5Context::update(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    size_t used = static_cast<size_t>((m_bits / 8) % 64);
    m_bits += static_cast<uint64_t>(size) * 8;
    while (size > 0) {
        const size_t take = std::min(size, 64 - used);
        std::memcpy(m_buffer + used, p, take);
        used += take;
        p += take;
        size -= take;
        if (used == 64) {
            block(m_buffer);
            used = 0;
        }
    }
}

std::string Md5Context::finalHex() {
    const uint64_t bits = m_bits;
    uint8_t pad[64] = {0x80};
    const size_t used = static_cast<size_t>((bits / 8) % 64);
    update(pad, used < 56 ? 56 - used : 120 - used);
    uint8_t length[8];
    for (int i = 0; i < 8; ++i) length[i] = static_cast<uint8_t>((bits >> (8 * i)) & 0xFF);
    update(length, 8);
    std::string out;
    for (uint32_t v : m_state) {
        for (int i = 0; i < 4; ++i) out += std::format("{:02x}", (v >> (8 * i)) & 0xFF);
    }
    // MD5Final leaves the context zeroed.
    std::memset(m_state, 0, sizeof m_state);
    m_bits = 0;
    std::memset(m_buffer, 0, sizeof m_buffer);
    return out;
}

std::string md5Hex(std::string_view bytes) {
    Md5Context md5;
    md5.update(bytes.data(), bytes.size());
    return md5.finalHex();
}

std::string loginHash(std::string_view passwordHash, std::string_view user, std::string_view first,
                      std::string_view second) {
    std::string text;
    text.append(passwordHash).append(kSpaces).append(user).append(first).append(second).append(passwordHash);
    return md5Hex(text);
}

std::string passwordEncode(std::string_view password) {
    g_passSeed = 0x79707367;
    std::string bytes;
    for (char c : password) {
        // (int)seed % 255: the seed is at most 2^31 here, so the sign plays no part but at that one value.
        const auto r = static_cast<int32_t>(passRandom()) % 0xFF;
        bytes.push_back(static_cast<char>(static_cast<uint8_t>(c) ^ static_cast<uint8_t>(r)));
    }
    return base64(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

std::string toUtf8(std::u16string_view text) {
    std::string out;
    for (char16_t c : text) {
        if (c == 0) break;
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>((c >> 6) | 0xC0));
            out.push_back(static_cast<char>((c & 0x3F) | 0x80));
        } else {
            out.push_back(static_cast<char>((c >> 12) | 0xE0));
            out.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
            out.push_back(static_cast<char>((c & 0x3F) | 0x80));
        }
    }
    return out;
}

// 0x00568BD0 for each character.
std::u16string fromUtf8(std::string_view text) {
    std::u16string out;
    size_t i = 0;
    while (i < text.size() && text[i] != '\0') {
        const auto b0 = static_cast<uint8_t>(text[i]);
        const uint8_t b1 = i + 1 < text.size() ? static_cast<uint8_t>(text[i + 1]) : 0;
        const uint8_t b2 = i + 2 < text.size() ? static_cast<uint8_t>(text[i + 2]) : 0;
        if (b0 < 0x80) {
            out.push_back(b0);
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0 && (b1 & 0xC0) == 0x80) {
            out.push_back(static_cast<char16_t>(((b0 & 0x1F) << 6) | (b1 & 0x3F)));
            i += 2;
        } else if ((b0 & 0xF0) == 0xE0 && (b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            out.push_back(static_cast<char16_t>(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F)));
            i += 3;
        } else {
            out.push_back(u'?');
            i += 1;
        }
    }
    return out;
}

// ---- The connection.

Connection::Connection(const Server& server) : m_server(server) {}

Result Connection::create(std::shared_ptr<Connection>& out, int32_t productId, int32_t namespaceId,
                          const Server& server) {
    out.reset();
    // __GSIACResult: the available check must have said available.
    if (availableResult() != AvailableCheck::Available) return Result::ParameterError;
    std::shared_ptr<Connection> c(new Connection(server));
    c->m_productId = productId;
    c->m_namespaceId = namespaceId;
    c->reset();
    WSADATA wsa{};
    WSAStartup(MAKEWORD(1, 1), &wsa);
    gameRandom().seed(GetTickCount());
    c->readInfoFile();
    out = std::move(c);
    return Result::NoError;
}

Connection::~Connection() {
    // gpDestroy: the logout when logged in; gp.info written when the state is not 0 (Stock problem: the game's
    // sign-out resets the connection before, so a sign-out while logged in writes nothing).
    disconnectInternal(true);
    if (m_infoCaching && m_state != State::None) writeInfoFile();
    // The transfers go (0x00576EA0: their files left open); the peers never do (Stock problem: they and their sockets
    // stay until the process ends).
    for (auto& peer : m_peers) static_cast<void>(peer.release());
}

void Connection::setError(std::string_view text) {
    m_errorString = cut(text, 0x100);
    m_errorStringW = fromUtf8(m_errorString);
}

void Connection::setError(std::string_view text, int32_t code) {
    setError(text);
    m_errorCode = code;
}

// gpiCallErrorCallback (0x0057C020): fatal marks the connection (disconnected at the end of the processing); the
// error's callback queued with the code and the text the connection holds when it is called.
void Connection::callError(Result result, bool fatal) {
    if (fatal) m_fatal = true;
    if (!onError) return;
    const int32_t code = m_errorCode;
    addCallback(1, 0, [this, result, code, fatal] {
        ErrorArg arg;
        arg.result = result;
        arg.errorCode = code;
        arg.errorString = m_errorStringW;
        arg.fatal = fatal;
        if (onError) onError(arg);
    });
}

void Connection::addCallback(int32_t type, int32_t operationId, std::function<void()> call) {
    m_callbacks.push_back(Pending{type, operationId, std::move(call)});
}

// 0x0057C280: none blocking, every callback waiting, those its own calls queue too; an operation blocking, only its
// own and the errors, the others kept after those queued meanwhile.
void Connection::processCallbacks(int32_t blockingId) {
    if (blockingId == 0) {
        while (!m_callbacks.empty()) {
            std::vector<Pending> list;
            list.swap(m_callbacks);
            for (Pending& p : list) p.call();
        }
        return;
    }
    std::vector<Pending> list;
    list.swap(m_callbacks);
    std::vector<Pending> kept;
    for (Pending& p : list) {
        if (p.operationId == blockingId || p.type == 1) {
            p.call();
        } else {
            kept.push_back(std::move(p));
        }
    }
    for (Pending& p : kept) m_callbacks.push_back(std::move(p));
}

Connection::Operation& Connection::addOperation(int32_t type, bool blocking) {
    auto op = std::make_unique<Operation>();
    op->type = type;
    op->blocking = blocking;
    if (type == 0) {
        op->id = 1;
    } else {
        op->id = m_nextOperationId++;
        if (m_nextOperationId < 2) m_nextOperationId = 2;
    }
    m_operations.insert(m_operations.begin(), std::move(op));
    return *m_operations.front();
}

Connection::Operation* Connection::findOperation(int32_t id) {
    for (auto& op : m_operations) {
        if (op->id == id) return op.get();
    }
    return nullptr;
}

void Connection::removeOperation(const Operation* op) {
    for (auto it = m_operations.begin(); it != m_operations.end(); ++it) {
        if (it->get() == op) {
            m_operations.erase(it);
            return;
        }
    }
}

// gpiFailedOpCallback (0x0057FC50): the operation's callback with its result; a connect's with the profile of a
// \pid\ when the server said the nick is taken (0x0201, the profile id then forgotten); a new profile's, a search's
// and a unique nick's with nothing more.
void Connection::failedCallback(const Operation& op) {
    const Result result = op.result;
    if (op.type == 0 && op.connectCallback) {
        ConnectResponse r;
        r.result = result;
        if (m_errorCode == kNewUserBadNick) {
            r.profile = m_profileId;
            m_profileId = 0;
        }
        ConnectCallback cb = op.connectCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    } else if (op.type == 2 && op.infoCallback) {
        GetInfoResponse r;
        r.result = result;
        InfoCallback cb = op.infoCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    } else if (op.type == 1 && op.profileCallback) {
        ProfileResponse r;
        r.result = result;
        auto cb = op.profileCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    } else if (op.type == 4 && op.resultCallback) {
        ResultResponse r;
        r.result = result;
        auto cb = op.resultCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    } else if (op.type == 3 && op.search && op.search->search.hasCallback()) {
        // A search's: 16 bytes with the result and zeros, whatever its kind (Stock problem: the kinds whose answer is
        // larger read past them); a profile search's "more" 0.
        const Search& s = op.search->search;
        std::function<void()> call;
        switch (s.type) {
        case 1: {
            auto cb = s.onProfiles;
            call = [cb, result] {
                ProfileSearchResponse r;
                r.result = result;
                cb(r);
            };
            break;
        }
        case 2: {
            auto cb = s.onValidEmail;
            call = [cb, result] { cb(IsValidEmailResponse{result, {}, 0}); };
            break;
        }
        case 3: {
            auto cb = s.onNicks;
            call = [cb, result] {
                UserNicksResponse r;
                r.result = result;
                cb(r);
            };
            break;
        }
        case 4: {
            auto cb = s.onPlayers;
            call = [cb, result] {
                FindPlayersResponse r;
                r.result = result;
                cb(r);
            };
            break;
        }
        case 5:
        case 6: {
            auto cb = s.onProfile;
            call = [cb, result] { cb(ProfileResponse{result, 0}); };
            break;
        }
        case 7: {
            auto cb = s.onOthers;
            call = [cb, result] {
                ReverseBuddiesResponse r;
                r.result = result;
                cb(r);
            };
            break;
        }
        case 8: {
            auto cb = s.onSuggested;
            call = [cb, result] {
                SuggestUniqueNickResponse r;
                r.result = result;
                cb(r);
            };
            break;
        }
        default:
            break;
        }
        if (call) addCallback(0, op.id, std::move(call));
    }
}

// ---- Profiles.

Profile* Connection::profile(int32_t profileId) {
    const int32_t bucket = profileId % 32;
    if (bucket < 0) return nullptr;
    for (auto& p : m_profiles[static_cast<size_t>(bucket)]) {
        if (p->profileId == profileId) return p.get();
    }
    return nullptr;
}

const Profile* Connection::findProfile(int32_t profileId) const {
    return const_cast<Connection*>(this)->profile(profileId);
}

Profile* Connection::addProfile(int32_t profileId) {
    if (profileId < 1) return nullptr;
    if (Profile* p = profile(profileId)) return p;
    auto p = std::make_unique<Profile>();
    p->profileId = profileId;
    m_profiles[static_cast<size_t>(profileId % 32)].push_back(std::move(p));
    ++m_profileCount;
    return m_profiles[static_cast<size_t>(profileId % 32)].back().get();
}

void Connection::removeProfile(int32_t profileId) {
    const int32_t bucket = profileId % 32;
    if (bucket < 0) return;
    auto& list = m_profiles[static_cast<size_t>(bucket)];
    for (auto it = list.begin(); it != list.end(); ++it) {
        if ((*it)->profileId == profileId) {
            list.erase(it);
            return;
        }
    }
}

template <typename F>
Profile* Connection::mapProfiles(F&& keepGoing) {
    for (auto& list : m_profiles) {
        for (size_t i = list.size(); i-- > 0;) {
            Profile* p = list[i].get();
            if (!keepGoing(*p)) return p;
        }
    }
    return nullptr;
}

Profile* Connection::buddyAt(int32_t index) {
    return mapProfiles([index](const Profile& p) { return !(p.buddy && p.buddy->index == index); });
}

// ---- gp.info.

namespace {

// 0x00575E80: a "key=value" line (each at most 512 characters, the value to the line's end); false at a line that
// is not one (an empty line, a line starting with '=', the file's end).
bool readKeyValue(FILE* f, std::string& key, std::string& value) {
    key.clear();
    value.clear();
    for (;;) {
        if (key.size() == 0x200) return false;
        const int c = std::fgetc(f);
        if (c == EOF || c == '\n') return false;
        if (c == '=') break;
        key.push_back(static_cast<char>(c));
    }
    if (key.empty()) return false;
    for (;;) {
        if (value.size() == 0x200) return false;
        int c = std::fgetc(f);
        if (c == EOF) c = '\n';
        if (c == '\n') break;
        value.push_back(static_cast<char>(c));
    }
    return true;
}

}  // namespace

// 0x00576640: "2" on the first line, then each profile (0x00575F30): "[<id>]" and its keys up to a line that is not
// one; the profile kept with its user id and, "valid" not 0, its info. "sex=Female" is Female in the fixed build (the
// stock compared the second letter with 'F': Pat).
void Connection::readInfoFile() {
    FILE* f = nullptr;
    if (fopen_s(&f, m_server.infoFile.c_str(), "rt") != 0 || f == nullptr) return;
    int version = 0;
    if (fscanf_s(f, "%d\n", &version) == 1 && version == 2) {
        for (;;) {
            int c = 0;
            do {
                c = std::fgetc(f);
            } while (c != EOF && c != '[');
            if (c == EOF) break;
            int32_t id = 0;
            if (fscanf_s(f, "%d]\n", &id) != 1) break;
            int32_t userId = 0;
            int32_t valid = 0;
            InfoCache info;
            std::string key;
            std::string value;
            while (readKeyValue(f, key, value)) {
                if (key == "userid") {
                    userId = atolOf(value);
                } else if (key == "nick") {
                    info.nick = cut(value, 0x1F);
                } else if (key == "uniquenick") {
                    info.uniquenick = cut(value, 0x33);
                } else if (key == "email") {
                    info.email = cut(value, 0x33);
                } else if (key == "firstname") {
                    info.firstname = cut(value, 0x1F);
                } else if (key == "lastname") {
                    info.lastname = cut(value, 0x1F);
                } else if (key == "homepage") {
                    info.homepage = cut(value, 0x4C);
                } else if (key == "icquin") {
                    info.icquin = atolOf(value);
                } else if (key == "zipcode") {
                    info.zipcode = cut(value, 0xB);
                } else if (key == "countrycode") {
                    info.countrycode = cut(value, 3);
                } else if (key == "birthday") {
                    info.birthday = atolOf(value);
                } else if (key == "birthmonth") {
                    info.birthmonth = atolOf(value);
                } else if (key == "birthyear") {
                    info.birthyear = atolOf(value);
                } else if (key == "sex") {
                    if (std::toupper(static_cast<unsigned char>(value.empty() ? '\0' : value[0])) == 'M') {
                        info.sex = 0x500;
                    } else {
                        info.sex = std::toupper(static_cast<unsigned char>(value.empty() ? '\0' : value[0])) == 'F'
                                       ? 0x501
                                       : 0x502;
                    }
                } else if (key == "publicmask") {
                    info.publicmask = atolOf(value);
                } else if (key == "aimname") {
                    info.aimname = cut(value, 0x33);
                } else if (key == "pic") {
                    info.pic = atolOf(value);
                } else if (key == "occupationid") {
                    info.occupationid = atolOf(value);
                } else if (key == "industryid") {
                    info.industryid = atolOf(value);
                } else if (key == "incomeid") {
                    info.incomeid = atolOf(value);
                } else if (key == "marriedid") {
                    info.marriedid = atolOf(value);
                } else if (key == "childcount") {
                    info.childcount = atolOf(value);
                } else if (key == "interests1") {
                    info.interests1 = atolOf(value);
                } else if (key == "ownership1") {
                    info.ownership1 = atolOf(value);
                } else if (key == "conntypeid") {
                    info.conntypeid = atolOf(value);
                } else if (key == "valid") {
                    valid = atolOf(value);
                }
            }
            if (Profile* p = addProfile(id)) {
                p->userId = userId;
                p->buddy.reset();
                p->authSig.reset();
                p->requestCount = 0;
                p->peerSig.reset();
                if (valid != 0 && m_infoCaching) p->cache = info;
            }
        }
    }
    std::fclose(f);
}

// 0x00576980: "2", then each profile in the table's order (0x005766E0): "[<id>]", "userid=" when it has one, with
// its info "valid=1" and the keys, then an empty line.
void Connection::writeInfoFile() {
    FILE* f = nullptr;
    if (fopen_s(&f, m_server.infoFile.c_str(), "wt") != 0 || f == nullptr) return;
    std::fprintf(f, "%d\n", 2);
    mapProfiles([f](const Profile& p) {
        std::fprintf(f, "[%d]\n", p.profileId);
        if (p.userId != 0) std::fprintf(f, "userid=%d\n", p.userId);
        if (p.cache) {
            const InfoCache& c = *p.cache;
            std::fprintf(f, "valid=1\n");
            std::fprintf(f, "nick=%s\n", c.nick.c_str());
            std::fprintf(f, "uniquenick=%s\n", c.uniquenick.c_str());
            std::fprintf(f, "email=%s\n", c.email.c_str());
            std::fprintf(f, "firstname=%s\n", c.firstname.c_str());
            std::fprintf(f, "lastname=%s\n", c.lastname.c_str());
            std::fprintf(f, "homepage=%s\n", c.homepage.c_str());
            std::fprintf(f, "icquin=%d\n", c.icquin);
            std::fprintf(f, "zipcode=%s\n", c.zipcode.c_str());
            std::fprintf(f, "countrycode=%s\n", c.countrycode.c_str());
            std::fprintf(f, "birthday=%d\n", c.birthday);
            std::fprintf(f, "birthmonth=%d\n", c.birthmonth);
            std::fprintf(f, "birthyear=%d\n", c.birthyear);
            if (c.sex == 0x500) std::fprintf(f, "sex=Male\n");
            if (c.sex == 0x501) std::fprintf(f, "sex=Female\n");
            if (c.sex == 0x502) std::fprintf(f, "sex=Pat\n");
            std::fprintf(f, "publicmask=%d\n", c.publicmask);
            std::fprintf(f, "aimname=%s\n", c.aimname.c_str());
            std::fprintf(f, "pic=%d\n", c.pic);
            std::fprintf(f, "occupationid=%d\n", c.occupationid);
            std::fprintf(f, "industryid=%d\n", c.industryid);
            std::fprintf(f, "incomeid=%d\n", c.incomeid);
            std::fprintf(f, "marriedid=%d\n", c.marriedid);
            std::fprintf(f, "childcount=%d\n", c.childcount);
            std::fprintf(f, "interests1=%d\n", c.interests1);
            std::fprintf(f, "ownership1=%d\n", c.ownership1);
            std::fprintf(f, "conntypeid=%d\n", c.conntypeid);
        }
        std::fprintf(f, "\n");
        return true;
    });
    std::fclose(f);
}

// ---- Connecting.

Result Connection::connect(std::u16string_view nick, std::u16string_view email, std::u16string_view password,
                           bool firewall, bool blocking, ConnectCallback callback) {
    const std::string n = narrow(nick);
    const std::string e = narrow(email);
    const std::string p = narrow(password);
    // gpConnectA (0x00568E60): the nick, the email and the password not empty, a callback ("No callback."); in the
    // simulation its callback at once with a response of zeros.
    if (n.empty() || e.empty() || p.empty()) return Result::ParameterError;
    if (!callback) {
        setError("No callback.");
        return Result::ParameterError;
    }
    if (m_simulation) {
        callback(ConnectResponse{});
        return Result::NoError;
    }
    return connectInternal(n, "", e, p, "", "", "", firewall, false, blocking, std::move(callback));
}

Result Connection::connectNewUser(std::u16string_view nick, std::u16string_view uniquenick, std::u16string_view email,
                                  std::u16string_view password, std::u16string_view cdkey, bool firewall,
                                  bool blocking, ConnectCallback callback) {
    const std::string n = narrow(nick);
    const std::string e = narrow(email);
    const std::string p = narrow(password);
    // The fixed build: the email whole (the stock narrowed it into 32 bytes followed by the password's on 0x00569160's
    // stack: from 32 characters on it ran into them and the password written after it took their place).
    // gpConnectNewUserA (0x00569030): the nick, the email and the password not empty, a callback; the simulation as
    // gpConnect's.
    if (n.empty() || e.empty() || p.empty()) return Result::ParameterError;
    if (!callback) {
        setError("No callback.");
        return Result::ParameterError;
    }
    if (m_simulation) {
        callback(ConnectResponse{});
        return Result::NoError;
    }
    return connectInternal(n, narrow(uniquenick), e, p, "", "", narrow(cdkey), firewall, true, blocking,
                           std::move(callback));
}

// gpiConnect (0x00573C80): a disconnected connection reset first; only one with no state connects: its nick (30
// characters), unique nick, email (50, in lower case) and password (30) kept, the connect operation (id 1) made and
// the connection manager reached; failing, the operation's callback with the result and the connection
// disconnected.
Result Connection::connectInternal(const std::string& nick, const std::string& uniquenick, const std::string& email,
                                   const std::string& password, const std::string& authToken,
                                   const std::string& partnerChallenge, const std::string& cdkey, bool firewall,
                                   bool newUser, bool blocking, ConnectCallback callback) {
    if (m_state == State::Disconnected) reset();
    if (m_state != State::None) {
        setError("Invalid connection.");
        return Result::ParameterError;
    }
    m_firewall = firewall;
    m_nick = cut(nick, 0x1F);
    m_uniquenick = cut(uniquenick, 0x33);
    m_email = cut(email, 0x33);
    m_password = cut(password, 0x1F);
    for (char& c : m_email) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    Operation& op = addOperation(0, blocking);
    op.newUser = newUser;
    if (!authToken.empty() && !partnerChallenge.empty()) {
        op.authToken = cut(authToken, 0x100);
        op.partnerChallenge = cut(partnerChallenge, 0x100);
    }
    op.cdkey = cut(cdkey, 0x41);
    op.connectCallback = std::move(callback);
    const Result r = connectToServer(op);
    if (r != Result::NoError) {
        op.result = r;
        failedCallback(op);
        disconnectInternal(false);
        return r;
    }
    if (op.blocking) {
        const Result p = processInternal(op.id);
        if (p != Result::NoError) return p;
    }
    return Result::NoError;
}

// 0x00573930: without the firewall a listening socket for the peers (non-blocking, any address, a free port, a
// backlog of 5: its port told in the login), with it none (the game always gives it: the port 0); the connection
// manager's socket (non-blocking), its name resolved and the connect begun: the operation waits for \lc\1, the state
// 1. Each failure is the network error (5, fatal) with its text.
Result Connection::connectToServer(Operation& op) {
    auto fail = [this](std::string_view text) {
        setError(text, kNetwork);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    if (!m_firewall) {
        if (!m_peerListener.listen(0, true, 5)) {
            const std::string_view step = m_peerListener.error();
            if (step == "socket") return fail("There was an error creating a socket.");
            if (step == "bind") return fail("There was an error binding a socket.");
            if (step == "listen") return fail("There was an error listening on a socket.");
            return fail("There was an error getting a socket's addres.");
        }
        m_peerPort = htons(m_peerListener.port());
    } else {
        m_peerListener.close();
        m_peerPort = 0;
    }
    if (!m_socket.open()) return fail("There was an error creating a socket.");
    const uint32_t ip = resolveHost(m_server.host);
    if (ip == 0) return fail("Could not resolve connection mananger host name.");
    if (!m_socket.connectTo(ip, m_server.port)) return fail("There was an error connecting a socket.");
    op.state = 1;
    m_state = State::Connecting;
    return Result::NoError;
}

// 0x00574CD0: the connect's progress; refused (the socket's error): 0x0107, fatal; made: the state 2.
Result Connection::checkConnect() {
    m_socket.poll();
    if (m_socket.state() == TcpConnection::State::Failed && !m_socket.lost()) {
        m_errorCode = kLoginConnectionFailed;
        callError(Result::ServerError, true);
        return Result::ServerError;
    }
    if (m_socket.connected() || m_socket.lost()) m_state = State::Connected;
    return Result::NoError;
}

Result Connection::process() {
    std::shared_ptr<Connection> self = shared_from_this();  // a callback may let the connection go
    return processInternal(0);
}

// gpiProcess (0x00579850): connecting, the connect's progress (a blocking connect waits here, Sleep(10)), a refusal
// giving the connect operation its result; connected or logged in, the connection manager's messages, then the peers
// (0x0057B980) and the transfers (0x00578C10), each without an error before; without an error the searches
// (connected or not, 0x00572FA0); the operations with a result have their callbacks and go; the callbacks waiting; a
// fatal error disconnects.
Result Connection::processInternal(int32_t blockingId) {
    Result r = Result::NoError;
    if (m_state == State::Connecting) {
        for (;;) {
            r = checkConnect();
            if (r != Result::NoError) break;
            if (blockingId == 0 || m_state != State::Connecting) break;
            Sleep(10);
        }
        if (r != Result::NoError) {
            if (Operation* op = findOperation(1)) op->result = Result::ServerError;
        }
    }
    if ((m_state == State::LoggedIn || m_state == State::Connected) && r == Result::NoError) {
        r = processConnectionManager();
        if (r == Result::NoError) r = processPeers();
        if (r == Result::NoError) r = processTransfers();
    }
    if (r == Result::NoError) r = processSearches();
    for (size_t i = 0; i < m_operations.size();) {
        if (m_operations[i]->result != Result::NoError) {
            failedCallback(*m_operations[i]);
            m_operations.erase(m_operations.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    processCallbacks(blockingId);
    if (m_fatal) disconnectInternal(false);
    return r;
}

void Connection::flush() {
    if (m_output.empty()) return;
    m_socket.send(m_output);
    m_output.clear();
}

// gpiProcessConnectionManager (0x005794B0): the profile's updates (0x0057AAE0), the bytes waiting sent, those come
// read; each message to its "\final\": with "\id\" to the operation of that id (the connect 0x00574690, a new
// profile 0x00576A10, the info 0x0057A2C0, a unique nick 0x0057C3A0; a search's none), its error ending the round;
// else an error (\error\, ending the round), a buddy's message (\bm\, 0x00575140) or the keep-alive (\ka\, nothing).
// The server closed: the state 4 and the error 7 (fatal). While an operation blocks, again after Sleep(10).
Result Connection::processConnectionManager() {
    for (;;) {
        sendInfoUpdates();
        flush();
        m_socket.poll();
        std::vector<uint8_t>& in = m_socket.received();
        if (!in.empty()) {
            m_input.append(reinterpret_cast<const char*>(in.data()), in.size());
            in.clear();
        }
        const bool closed = m_socket.lost();
        for (size_t final = cString(m_input).find(kFinal); final != std::string_view::npos;
             final = cString(m_input).find(kFinal)) {
            const std::string message = m_input.substr(0, final);
            m_input.erase(0, final + kFinal.size());
            const size_t idAt = message.find("\\id\\");
            if (idAt == std::string::npos) {
                if (checkForError(message, true)) return Result::ServerError;
                if (message.compare(0, 4, "\\bm\\") == 0) {
                    const Result r = processBuddyMessage(message);
                    if (r != Result::NoError) return r;
                }
                continue;
            }
            const int32_t id = std::atol(message.c_str() + idAt + 4);
            Operation* op = findOperation(id);
            if (op == nullptr) continue;
            Result r = Result::NoError;
            if (op->type == 0) {
                r = processConnect(*op, message);
            } else if (op->type == 1) {
                r = processNewProfile(*op, message);
            } else if (op->type == 2) {
                r = processGetInfo(*op, message);
            } else if (op->type == 4) {
                r = processUniqueNick(*op, message);
            }
            if (r != Result::NoError) {
                op->result = r;
                return r;
            }
        }
        if (closed) {
            m_state = State::Disconnected;
            m_errorCode = kConnectionClosed;
            callError(Result::NetworkError, true);
            return Result::NoError;
        }
        bool blocking = false;
        for (const auto& op : m_operations) {
            if (op->blocking && op->type != 3) blocking = true;
        }
        if (!blocking) return Result::NoError;
        Sleep(10);
    }
}

// gpiCheckForError (0x00579A10): a message beginning "\error\": its \err\ the code, its \errmsg\ the text (or none);
// with callback the error's callback, fatal when "\fatal\" is in it.
bool Connection::checkForError(const std::string& message, bool callback) {
    if (message.compare(0, 7, "\\error\\") != 0) return false;
    std::string value;
    if (valueForKey(message, "\\err\\", value, 0x10)) m_errorCode = atolOf(value);
    if (!valueForKey(message, "\\errmsg\\", m_errorString, 0x100)) m_errorString.clear();
    m_errorStringW = fromUtf8(m_errorString);
    if (callback) callError(Result::ServerError, message.find("\\fatal\\") != std::string::npos);
    return true;
}

// gpiProcessConnect (0x00574690). An error: the profile deleted (0x0106) forgets the profile and the ids, the nick
// taken (0x0201) keeps the \pid\; the error's callback (fatal with "\fatal\"); the server's error result. Else by the
// operation's step: \lc\1 with its \challenge\ sends \newuser\ (the step 3) or \login\ (2); \nur\ with \userid\ and
// \profileid\ sends \login\ (2); \lc\2 with \sesskey\, \userid\, \profileid\ and \proof\ checks the proof (0x0108,
// fatal, when it is not the server's), keeps the profile with its user id, logs in (the state 3) and answers the
// operation. Anything else: the parse error (1, fatal).
Result Connection::processConnect(Operation& op, const std::string& message) {
    if (checkForError(message, false)) {
        if (m_errorCode == kLoginProfileDeleted && m_profileId != 0) {
            removeProfile(m_profileId);
            m_userId = 0;
            m_profileId = 0;
        } else if (m_errorCode == kNewUserBadNick) {
            std::string value;
            if (valueForKey(message, "\\pid\\", value, 0x200)) m_profileId = atolOf(value);
        }
        callError(Result::ServerError, message.find("\\fatal\\") != std::string::npos);
        return Result::ServerError;
    }
    auto parseError = [this] {
        m_errorCode = kParse;
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    std::string value;
    if (op.state == 1) {
        if (message.compare(0, 5, "\\lc\\1") != 0 || !valueForKey(message, "\\challenge\\", op.serverChallenge, 0x80)) {
            return parseError();
        }
        if (op.newUser) {
            sendNewUser(op);
            op.state = 3;
        } else {
            sendLogin(op);
            op.state = 2;
        }
        return Result::NoError;
    }
    if (op.state == 3) {
        if (message.compare(0, 5, "\\nur\\") != 0) return parseError();
        if (!valueForKey(message, "\\userid\\", value, 0x200)) return parseError();
        m_userId = atolOf(value);
        if (!valueForKey(message, "\\profileid\\", value, 0x200)) return parseError();
        m_profileId = atolOf(value);
        sendLogin(op);
        op.state = 2;
        return Result::NoError;
    }
    if (op.state != 2) return Result::NoError;
    if (message.compare(0, 5, "\\lc\\2") != 0) return parseError();
    if (!valueForKey(message, "\\sesskey\\", value, 0x200)) return parseError();
    m_sessKey = atolOf(value);
    if (!valueForKey(message, "\\userid\\", value, 0x200)) return parseError();
    m_userId = atolOf(value);
    if (!valueForKey(message, "\\profileid\\", value, 0x200)) return parseError();
    m_profileId = atolOf(value);
    std::string uniquenick;
    if (!valueForKey(message, "\\uniquenick\\", uniquenick, 0x33)) uniquenick.clear();
    if (!valueForKey(message, "\\lt\\", m_loginTicket, 0x19)) m_loginTicket.clear();
    std::string user = op.authToken;
    if (user.empty()) user = m_uniquenick;
    if (user.empty()) user = m_nick + "@" + m_email;
    const std::string proof = loginHash(op.passwordHash, user, op.serverChallenge, op.clientChallenge);
    if (!valueForKey(message, "\\proof\\", value, 0x200)) return parseError();
    if (value.size() < 32 || proof.compare(0, 32, value, 0, 32) != 0) {
        m_errorCode = kLoginServerAuthFailed;
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    }
    if (m_infoCaching) {
        if (Profile* p = addProfile(m_profileId)) {
            p->profileId = m_profileId;
            p->userId = m_userId;
        }
    }
    m_state = State::LoggedIn;
    if (op.connectCallback) {
        ConnectResponse r;
        r.result = Result::NoError;
        r.profile = m_profileId;
        r.uniquenick = fromUtf8(uniquenick);
        ConnectCallback cb = op.connectCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    }
    removeOperation(&op);
    return Result::NoError;
}

// 0x00573F00: the client's challenge (32 of "A-Za-z0-9" by the C runtime's rand() % 62), the password's MD5 (or the
// partner challenge's with an auth token), the profile's ids when gp.info knows the nick and email (0x00576D70), and
// \login\: \challenge\, the user (\authtoken\, \uniquenick\ or \user\ nick@email), [\userid\, \profileid\],
// \response\, \firewall\1 when behind one, \port\ (the peers' port: 0), \productid\, \gamename\ (the available
// check's), \namespaceid\, \id\1.
void Connection::sendLogin(Operation& op) {
    static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    op.clientChallenge.clear();
    for (int i = 0; i < 32; ++i) {
        op.clientChallenge.push_back(kChars[static_cast<uint32_t>(gameRandom().next()) % 62]);
    }
    op.passwordHash = md5Hex(op.partnerChallenge.empty() ? m_password : op.partnerChallenge);
    std::string user = op.authToken;
    if (user.empty()) user = m_uniquenick;
    if (user.empty()) user = m_nick + "@" + m_email;
    const std::string response = loginHash(op.passwordHash, user, op.clientChallenge, op.serverChallenge);
    if (m_infoCaching) {
        if (const Profile* p = mapProfiles([this](const Profile& p) {
                return !(p.cache && p.cache->nick == m_nick && p.cache->email == m_email);
            })) {
            m_userId = p->userId;
            m_profileId = p->profileId;
        }
    }
    std::string m = "\\login\\\\challenge\\" + op.clientChallenge;
    if (!op.authToken.empty()) {
        m += "\\authtoken\\" + op.authToken;
    } else if (!m_uniquenick.empty()) {
        m += "\\uniquenick\\" + m_uniquenick;
    } else {
        m += "\\user\\" + m_nick + "@" + m_email;
    }
    if (m_userId != 0) m += std::format("\\userid\\{}", m_userId);
    if (m_profileId != 0) m += std::format("\\profileid\\{}", m_profileId);
    m += "\\response\\" + response;
    if (m_firewall) m += "\\firewall\\1";
    m += std::format("\\port\\{}", static_cast<int16_t>(m_peerPort));
    m += std::format("\\productid\\{}", m_productId);
    m += "\\gamename\\" + availableGameName();
    m += std::format("\\namespaceid\\{}", m_namespaceId);
    m += "\\id\\1";
    m += kFinal;
    m_output += m;
}

// 0x005742E0: \newuser\: \email\, \nick\, \passwordenc\ (the password encoded), \productid\, \gamename\,
// \namespaceid\, \uniquenick\, [\cdkeyenc\], \id\1.
void Connection::sendNewUser(Operation& op) {
    std::string m = "\\newuser\\\\email\\" + m_email;
    m += "\\nick\\" + m_nick;
    m += "\\passwordenc\\" + passwordEncode(m_password);
    m += std::format("\\productid\\{}", m_productId);
    m += "\\gamename\\" + availableGameName();
    m += std::format("\\namespaceid\\{}", m_namespaceId);
    m += "\\uniquenick\\" + m_uniquenick;
    if (!op.cdkey.empty()) m += "\\cdkeyenc\\" + passwordEncode(op.cdkey);
    m += "\\id\\1";
    m += kFinal;
    m_output += m;
}

// gpiDisconnect (0x00574E00): connected, \logout\ when asked and logged in, the bytes waiting sent, the socket and
// the peers' listening one closed, the state 4 and the ids 0; the buffers, the operations and the buddies' and
// requests' parts of the profiles let go, the profiles without info forgotten (the peers stay).
void Connection::disconnectInternal(bool logout) {
    if (m_state == State::Disconnected) return;
    if (m_state != State::None) {
        if (logout && m_state == State::LoggedIn) m_output += std::format("\\logout\\\\sesskey\\{}\\final\\", m_sessKey);
        flush();
        m_socket.close();
        m_peerListener.close();
        m_state = State::Disconnected;
        m_userId = 0;
        m_profileId = 0;
    }
    m_input.clear();
    m_output.clear();
    m_operations.clear();
    for (auto& list : m_profiles) {
        for (size_t i = list.size(); i-- > 0;) {
            Profile& p = *list[i];
            p.buddy.reset();
            p.authSig.reset();
            p.peerSig.reset();
            p.requestCount = 0;
            if (!p.cache) list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

// gpiReset (0x005792C0): the strings, the state 0, the buffers, the operations (from the id 2), the buddies (none,
// their count 0, each profile's buddy, request and peer parts dropped), the last status -1, the ids, the fatal flag.
void Connection::reset() {
    m_nick.clear();
    m_uniquenick.clear();
    m_email.clear();
    m_socket.close();
    m_state = State::None;
    m_input.clear();
    m_output.clear();
    m_nextOperationId = 2;
    m_operations.clear();
    m_buddyCount = 0;
    mapProfiles([](Profile& p) {
        p.buddy.reset();
        p.authSig.reset();
        p.requestCount = 0;
        p.peerSig.reset();
        return true;
    });
    m_lastStatus = -1;
    m_userId = 0;
    m_profileId = 0;
    m_sessKey = 0;
    m_fatal = false;
    m_lastStatusString.clear();
    m_lastLocationString.clear();
}

void Connection::disconnect() {
    disconnectInternal(true);
    reset();
}

// ---- After the login.

// gpiGetInfo (0x0057AC30): with checkCache (and the cache on) a profile with info answers at once (the operation
// made and gone, its callback queued); else \getprofile\\sesskey\..\profileid\..\id\<op>; blocking, processed until
// the operation's callback came.
Result Connection::getInfo(int32_t profileId, bool checkCache, bool blocking, InfoCallback callback) {
    // gpGetInfo (0x005692C0): a profile, a callback ("No callback."); in the simulation its callback at once with an
    // info of zeros.
    if (profileId == 0) return Result::ParameterError;
    if (!callback) {
        setError("No callback.");
        return Result::ParameterError;
    }
    if (m_simulation) {
        callback(GetInfoResponse{});
        return Result::NoError;
    }
    if (m_state == State::Disconnected) return Result::ParameterError;
    std::shared_ptr<Connection> self = shared_from_this();
    int32_t id = 0;
    const Profile* cached = checkCache && m_infoCaching ? profile(profileId) : nullptr;
    if (cached != nullptr && cached->cache) {
        const InfoCache& c = *cached->cache;
        GetInfoResponse r;
        r.result = Result::NoError;
        r.profile = profileId;
        r.nick = fromUtf8(c.nick);
        r.uniquenick = fromUtf8(c.uniquenick);
        r.email = fromUtf8(c.email);
        r.firstname = fromUtf8(c.firstname);
        r.lastname = fromUtf8(c.lastname);
        r.homepage = fromUtf8(c.homepage);
        r.icquin = c.icquin;
        r.zipcode = fromUtf8(c.zipcode);
        r.countrycode = fromUtf8(c.countrycode);
        r.longitude = c.longitude;
        r.latitude = c.latitude;
        r.place = fromUtf8(c.place);
        r.birthday = c.birthday;
        r.birthmonth = c.birthmonth;
        r.birthyear = c.birthyear;
        r.sex = c.sex;
        r.publicmask = c.publicmask;
        r.aimname = fromUtf8(c.aimname);
        r.pic = c.pic;
        r.occupationid = c.occupationid;
        r.industryid = c.industryid;
        r.incomeid = c.incomeid;
        r.marriedid = c.marriedid;
        r.childcount = c.childcount;
        r.interests1 = c.interests1;
        r.ownership1 = c.ownership1;
        r.conntypeid = c.conntypeid;
        Operation& op = addOperation(2, true);
        id = op.id;
        addCallback(0, id, [callback, r] { callback(r); });
        removeOperation(&op);
    } else {
        Operation& op = addOperation(2, blocking);
        op.infoCallback = std::move(callback);
        id = op.id;
        m_output += std::format("\\getprofile\\\\sesskey\\{}\\profileid\\{}\\id\\{}\\final\\", m_sessKey, profileId, id);
    }
    if (blocking) return processInternal(id);
    return Result::NoError;
}

// 0x0057A2C0: an error ends it (its callback); a message not "\pi\", or without \profileid\ or \sig\: the parse error
// (fatal). The info read (each key anywhere in the message, its default when missing), the cache on: kept as the
// profile's (with its \sig\, kept too for the peers of the profile it wakes), and the operation answered and gone.
Result Connection::processGetInfo(Operation& op, const std::string& message) {
    if (checkForError(message, true)) return Result::ServerError;
    auto parseError = [this] {
        m_errorCode = kParse;
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    if (message.compare(0, 4, "\\pi\\") != 0) return parseError();
    std::string value;
    if (!valueForKey(message, "\\profileid\\", value, 0x40)) return parseError();
    const int32_t profileId = atolOf(value);
    InfoCache c;
    if (!valueForKey(message, "\\nick\\", c.nick, 0x1F)) c.nick.clear();
    if (!valueForKey(message, "\\uniquenick\\", c.uniquenick, 0x33)) c.uniquenick.clear();
    if (!valueForKey(message, "\\email\\", c.email, 0x33)) c.email.clear();
    if (!valueForKey(message, "\\firstname\\", c.firstname, 0x1F)) c.firstname.clear();
    if (!valueForKey(message, "\\lastname\\", c.lastname, 0x1F)) c.lastname.clear();
    c.icquin = valueForKey(message, "\\icquin\\", value, 0x40) ? atolOf(value) : -1;
    if (!valueForKey(message, "\\homepage\\", c.homepage, 0x4C)) c.homepage.clear();
    if (!valueForKey(message, "\\zipcode\\", c.zipcode, 0xB)) c.zipcode.clear();
    if (!valueForKey(message, "\\countrycode\\", c.countrycode, 3)) c.countrycode.clear();
    c.longitude = valueForKey(message, "\\lon\\", value, 0x40) ? static_cast<float>(std::atof(value.c_str())) : 0.0f;
    c.latitude = valueForKey(message, "\\lat\\", value, 0x40) ? static_cast<float>(std::atof(value.c_str())) : 0.0f;
    if (!valueForKey(message, "\\loc\\", c.place, 0x80)) c.place.clear();
    if (valueForKey(message, "\\birthday\\", value, 0x40)) {
        // 0x0057A030: the day (bits 24 ..), the month (16 .. 23) and the year (0 .. 15) of a date that is one.
        const auto v = static_cast<uint32_t>(atolOf(value));
        const auto day = static_cast<int32_t>(v >> 24);
        const auto month = static_cast<int32_t>((v >> 16) & 0xFF);
        const auto year = static_cast<int32_t>(v & 0xFFFF);
        static const int kDays[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        bool valid = day == 0 && month == 0 && year == 0;
        if (!valid && day >= 0 && month >= 0 && month <= 12 && year >= 0) {
            int most = kDays[month];
            if (month == 2) most = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;
            const bool dayOk = month == 0 ? day == 0 : day <= most;
            valid = dayOk && year > 1899 && year < 2080 &&
                    (year != 2079 || month < 6 || (month == 6 && day < 7));
        }
        if (!valid) return Result::ParameterError;
        c.birthday = day;
        c.birthmonth = month;
        c.birthyear = year;
    }
    if (!valueForKey(message, "\\sex\\", value, 0x40)) {
        c.sex = 0x502;
    } else if (!value.empty() && value[0] == '0') {
        c.sex = 0x500;
    } else {
        c.sex = (!value.empty() && value[0] == '1') ? 0x501 : 0x502;
    }
    c.publicmask = valueForKey(message, "\\pmask\\", value, 0x40) ? atolOf(value) : -1;
    if (!valueForKey(message, "\\aim\\", c.aimname, 0x33)) c.aimname.clear();
    auto number = [&](std::string_view key) { return valueForKey(message, key, value, 0x40) ? atolOf(value) : 0; };
    c.pic = number("\\pic\\");
    c.occupationid = number("\\occ\\");
    c.industryid = number("\\ind\\");
    c.incomeid = number("\\inc\\");
    c.marriedid = number("\\mar\\");
    c.childcount = number("\\chc\\");
    c.interests1 = number("\\i1\\");
    c.ownership1 = number("\\o1\\");
    c.conntypeid = number("\\conn\\");
    if (!valueForKey(message, "\\sig\\", value, 0x40)) return parseError();
    // The peers of the profile waiting for its signature go on to connect: it is kept then, the cache on or not.
    Profile* p = profile(profileId);
    bool keepSig = m_infoCaching;
    for (const auto& peer : m_peers) {
        if (peer->profile != profileId || peer->state != 0x65) continue;
        if (p == nullptr) p = addProfile(profileId);
        peer->state = 0x66;
        keepSig = true;
    }
    if (p == nullptr && m_infoCaching) p = addProfile(profileId);
    if (keepSig && p != nullptr) p->peerSig = value;
    if (m_infoCaching && p != nullptr) p->cache = c;
    if (op.infoCallback) {
        GetInfoResponse r;
        r.result = Result::NoError;
        r.profile = profileId;
        r.nick = fromUtf8(c.nick);
        r.uniquenick = fromUtf8(c.uniquenick);
        r.email = fromUtf8(c.email);
        r.firstname = fromUtf8(c.firstname);
        r.lastname = fromUtf8(c.lastname);
        r.homepage = fromUtf8(c.homepage);
        r.icquin = c.icquin;
        r.zipcode = fromUtf8(c.zipcode);
        r.countrycode = fromUtf8(c.countrycode);
        r.longitude = c.longitude;
        r.latitude = c.latitude;
        r.place = fromUtf8(c.place);
        r.birthday = c.birthday;
        r.birthmonth = c.birthmonth;
        r.birthyear = c.birthyear;
        r.sex = c.sex;
        r.publicmask = c.publicmask;
        r.aimname = fromUtf8(c.aimname);
        r.pic = c.pic;
        r.occupationid = c.occupationid;
        r.industryid = c.industryid;
        r.incomeid = c.incomeid;
        r.marriedid = c.marriedid;
        r.childcount = c.childcount;
        r.interests1 = c.interests1;
        r.ownership1 = c.ownership1;
        r.conntypeid = c.conntypeid;
        InfoCallback cb = op.infoCallback;
        addCallback(0, op.id, [cb, r] { cb(r); });
    }
    removeOperation(&op);
    return Result::NoError;
}

// gpiProcessRecvBuddyMessage (0x00575140): \bm\ (the type) and \f\ (the profile) or the parse error (fatal); \date\
// or now. 1 a message (its callback, not set by the game); 2 a request (\msg\ "<reason>|signed|<32 characters>": the
// signature kept for \authadd\, the profile's requests counted; its callback); 100 a status (\msg\ |s| the status,
// |ss|, |ls|, |ip| (host order) and |p|: the profile made a buddy, the next place of the list, at its first; its
// callback); 101 a game invite (|p| the product, |l| the location to the end; its callback); 102 a ping answered by
// 103 (0x00575A00: through a peer when the buddy has a port, which no player of the game has, else the server); 103
// a pong through the server: the transfers waiting for it have no connection (0x00578D00).
Result Connection::processBuddyMessage(const std::string& message) {
    auto parseError = [this] {
        m_errorCode = kParse;
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    std::string value;
    if (!valueForKey(message, "\\bm\\", value, 0x1000)) return parseError();
    const int32_t type = atolOf(value);
    if (!valueForKey(message, "\\f\\", value, 0x1000)) return parseError();
    const int32_t from = atolOf(value);
    const auto date = valueForKey(message, "\\date\\", value, 0x1000) ? static_cast<uint32_t>(atolOf(value))
                                                                       : static_cast<uint32_t>(std::time(nullptr));
    std::string msg;
    switch (type) {
    case 1: {
        if (!onBuddyMessage) return Result::NoError;
        if (!valueForKey(message, "\\msg\\", msg, 0x1000)) return parseError();
        BuddyMessageArg arg{from, date, fromUtf8(msg)};
        addCallback(2, 0, [this, arg] {
            if (onBuddyMessage) onBuddyMessage(arg);
        });
        return Result::NoError;
    }
    case 2: {
        Profile* p = addProfile(from);
        if (p == nullptr) return Result::MemoryError;
        if (!valueForKey(message, "\\msg\\", msg, 0x1000)) return parseError();
        const size_t signedAt = msg.find("|signed|");
        if (signedAt == std::string::npos) return parseError();
        const std::string sig = msg.substr(signedAt + 8);
        msg.resize(signedAt);
        if (sig.size() != 32) return parseError();
        p->authSig = sig;
        ++p->requestCount;
        if (!onBuddyRequest) return Result::NoError;
        BuddyRequestArg arg{from, date, fromUtf8(msg)};
        addCallback(6, 0, [this, arg] {
            if (onBuddyRequest) onBuddyRequest(arg);
        });
        return Result::NoError;
    }
    case 100: {
        Profile* p = addProfile(from);
        if (p == nullptr) return Result::MemoryError;
        if (!p->buddy) {
            p->buddy = Buddy{};
            p->buddy->index = m_buddyCount++;
        }
        if (!valueForKey(message, "\\msg\\", msg, 0x1000)) return parseError();
        if (!valueForKey(msg, "|s|", value, 0x10)) return parseError();
        Buddy& b = *p->buddy;
        b.status = atolOf(value);
        if (!valueForKey(msg, "|ss|", b.statusString, 0x100)) b.statusString.clear();
        if (!valueForKey(msg, "|ls|", b.locationString, 0x100)) b.locationString.clear();
        b.ip = valueForKey(msg, "|ip|", value, 0x10) ? htonl(static_cast<u_long>(atolOf(value))) : 0;
        b.port = valueForKey(msg, "|p|", value, 0x10) ? htons(static_cast<u_short>(atolOf(value))) : 0;
        if (!onBuddyStatus) return Result::NoError;
        BuddyStatusArg arg{from, date, b.index};
        addCallback(5, 0, [this, arg] {
            if (onBuddyStatus) onBuddyStatus(arg);
        });
        return Result::NoError;
    }
    case 101: {
        if (!valueForKey(message, "\\msg\\", msg, 0x1000)) return parseError();
        const size_t productAt = msg.find("|p|");
        if (productAt == std::string::npos || productAt + 3 >= msg.size()) return parseError();
        const int32_t product = std::atol(msg.c_str() + productAt + 3);
        const size_t locationAt = msg.find("|l|");
        const std::string location = locationAt == std::string::npos ? std::string() : cut(msg.substr(locationAt + 3), 0x100);
        if (!onGameInvite) return Result::NoError;
        GameInviteArg arg{from, product, widenBytes(location)};
        addCallback(0, 0, [this, arg] {
            if (onGameInvite) onGameInvite(arg);
        });
        return Result::NoError;
    }
    case 102:
        if (!valueForKey(message, "\\msg\\", msg, 0x1000)) return parseError();
        routeBuddyMessage(from, 103, "1");
        return Result::NoError;
    case 103:
        transfersPong(from, nullptr);
        return Result::NoError;
    default:
        return Result::NoError;
    }
}

// 0x00575910: \bm\<type>\sesskey\..\t\<profile>\msg\<text>\final\ (the text at most 3500 characters).
void Connection::sendBuddyMessage(int32_t profileId, int32_t type, const std::string& text) {
    m_output += std::format("\\bm\\{}\\sesskey\\{}\\t\\{}\\msg\\{}\\final\\", type, m_sessKey, profileId,
                            cut(text, 0xDAE));
}

Result Connection::setStatus(int32_t status, std::u16string_view statusString, std::u16string_view locationString) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    const std::string ss = slashes(cut(toUtf8(statusString), 0x100));
    const std::string ls = slashes(cut(toUtf8(locationString), 0x100));
    if (status == m_lastStatus && ss == m_lastStatusString && ls == m_lastLocationString) return Result::NoError;
    m_lastStatus = status;
    // The fixed build keeps the strings sent (the stock's +0x234 and +0x334 stayed as gpiReset left them, empty: the
    // same status with its strings went again at each call, every 60 s).
    m_lastStatusString = ss;
    m_lastLocationString = ls;
    m_output += std::format("\\status\\{}\\sesskey\\{}\\statstring\\{}\\locstring\\{}\\final\\", status, m_sessKey, ss,
                            ls);
    return Result::NoError;
}

Result Connection::setInvitableGames(const std::vector<int32_t>& products) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    std::string m = std::format("\\inviteto\\\\sesskey\\{}\\products\\", m_sessKey);
    for (size_t i = 0; i < products.size(); ++i) {
        m += std::format("{}", products[i]);
        if (i + 1 < products.size()) m += ",";
    }
    m += kFinal;
    m_output += m;
    return Result::NoError;
}

Result Connection::sendBuddyRequest(int32_t profileId, std::u16string_view reason) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    const std::string r = slashes(cut(toUtf8(reason), 0x401));
    m_output += std::format("\\addbuddy\\\\sesskey\\{}\\newprofileid\\{}\\reason\\{}\\final\\", m_sessKey, profileId, r);
    return Result::NoError;
}

// 0x00575AE0 (0x005750A0): the profile's request signature or a parameter error; \authadd\; the request counted
// down (its signature let go off the cache only).
Result Connection::authBuddyRequest(int32_t profileId) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    Profile* p = profile(profileId);
    if (p == nullptr || !p->authSig) return Result::ParameterError;
    m_output += std::format("\\authadd\\\\sesskey\\{}\\fromprofileid\\{}\\sig\\{}\\final\\", m_sessKey, profileId,
                            *p->authSig);
    --p->requestCount;
    if (!m_infoCaching && p->requestCount < 1) {
        p->authSig.reset();
        if (!p->cache && !p->buddy && !p->peerSig) removeProfile(profileId);
    }
    return Result::NoError;
}

Result Connection::denyBuddyRequest(int32_t profileId) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    Profile* p = profile(profileId);
    if (p == nullptr) return Result::NoError;
    --p->requestCount;
    if (!m_infoCaching && p->requestCount < 1) {
        p->authSig.reset();
        if (!p->cache && !p->buddy && !p->peerSig) removeProfile(profileId);
    }
    return Result::NoError;
}

Result Connection::deleteBuddy(int32_t profileId) {
    Profile* p = profile(profileId);
    if (p == nullptr || !p->buddy) return Result::ParameterError;
    m_output += std::format("\\delbuddy\\\\sesskey\\{}\\delprofileid\\{}\\final\\", m_sessKey, profileId);
    const int32_t index = p->buddy->index;
    p->buddy.reset();
    if (!p->cache && !p->peerSig && !p->authSig) removeProfile(profileId);
    --m_buddyCount;
    // 0x00575BE0: the buddies after it one place up.
    mapProfiles([index](Profile& q) {
        if (q.buddy && q.buddy->index > index) --q.buddy->index;
        return true;
    });
    return Result::NoError;
}

Result Connection::invitePlayer(int32_t profileId, int32_t productId, std::u16string_view location) {
    if (m_simulation) return Result::NoError;
    if (m_state == State::Disconnected) return Result::ParameterError;
    const std::string l = narrow(location);
    std::string m = std::format("\\pinvite\\\\sesskey\\{}\\profileid\\{}\\productid\\{}", m_sessKey, profileId, productId);
    if (!l.empty()) m += "\\location\\" + l;
    m += kFinal;
    m_output += m;
    return Result::NoError;
}

Result Connection::getBuddyStatus(int32_t index, BuddyStatus& out) {
    if (m_simulation) {
        out = BuddyStatus{};
        return Result::NoError;
    }
    if (index < 0 || index >= m_buddyCount) return Result::ParameterError;
    const Profile* p = buddyAt(index);
    if (p == nullptr) return Result::ParameterError;
    out.profile = p->profileId;
    out.status = p->buddy->status;
    out.statusString = fromUtf8(p->buddy->statusString);
    out.locationString = fromUtf8(p->buddy->locationString);
    out.ip = p->buddy->ip;
    out.port = p->buddy->port;
    return Result::NoError;
}

}  // namespace oj::net::gp
