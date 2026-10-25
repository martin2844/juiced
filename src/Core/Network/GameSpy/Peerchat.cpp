#include "Core/Network/GameSpy/Peerchat.h"

#include "Core/Base/Log.h"
#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <memory>
#include <utility>
#include <vector>

namespace oj::net::peerchat {

namespace {

std::u16string fromUtf8(std::string_view text) {
    return gp::fromUtf8(text);
}

// ":nick!user@host" → "nick".
std::string prefixNick(std::string_view prefix) {
    if (!prefix.empty() && prefix[0] == ':') prefix.remove_prefix(1);
    const size_t bang = prefix.find('!');
    return std::string(prefix.substr(0, bang));
}

bool sameText(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// The words of a line, the one after " :" whole (the trailing parameter).
std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at < line.size()) {
        if (line[at] == ' ') {
            ++at;
            continue;
        }
        if (line[at] == ':' && !out.empty()) {
            out.push_back(line.substr(at + 1));
            break;
        }
        const size_t end = line.find(' ', at);
        out.push_back(line.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return out;
}

}  // namespace

void Cipher::start(std::string_view challenge, std::string_view secretKey) {
    std::array<uint8_t, 16> key{};
    for (size_t i = 0; i < key.size(); ++i) {
        const auto c = i < challenge.size() ? static_cast<uint8_t>(challenge[i]) : uint8_t{0};
        const auto k = secretKey.empty() ? uint8_t{0} : static_cast<uint8_t>(secretKey[i % secretKey.size()]);
        key[i] = static_cast<uint8_t>(c ^ k);
    }
    for (size_t i = 0; i < m_table.size(); ++i) m_table[i] = static_cast<uint8_t>(255 - i);
    uint8_t j = 0;
    for (size_t i = 0; i < m_table.size(); ++i) {
        j = static_cast<uint8_t>(j + key[i % key.size()] + m_table[i]);
        std::swap(m_table[i], m_table[j]);
    }
    m_i = 0;
    m_j = 0;
}

void Cipher::run(uint8_t* data, size_t size) {
    for (size_t n = 0; n < size; ++n) {
        ++m_i;
        const uint8_t t = m_table[m_i];
        m_j = static_cast<uint8_t>(m_j + t);
        m_table[m_i] = m_table[m_j];
        m_table[m_j] = t;
        data[n] ^= m_table[static_cast<uint8_t>(t + m_table[m_i])];
    }
}

std::string chatNick(std::u16string_view nick) {
    std::string out;
    for (char16_t c : nick) {
        if (out.size() == 20) break;
        const bool ok = (c >= u'A' && c <= u'}') || (c >= u'0' && c <= u'9') || c == u'-';
        out.push_back(ok ? static_cast<char>(c) : '_');
    }
    if (out.empty()) return "Juiced";
    if ((out[0] >= '0' && out[0] <= '9') || out[0] == '-') out.insert(out.begin(), 'J');
    if (out.size() > 20) out.resize(20);
    return out;
}

std::string sessionChannel(std::string_view gameName, uint32_t sessionId) {
    return std::format("#GSP!{}!{:08X}", gameName, sessionId);
}

void LobbyChat::start(std::u16string_view nick, const std::string& channel, const std::string& gameName,
                      const std::string& secretKey, const Server& server) {
    stop();
    m_lines.clear();
    m_unread = 0;
    m_nick = chatNick(nick);
    m_nickTries = 0;
    m_channel = channel;
    m_gameName = gameName;
    m_secretKey = secretKey;
    m_plain.clear();
    m_input.clear();
    const std::string host = server.host.empty() ? std::string("peerchat.") + kServiceDomain : server.host;
    const uint32_t ip = resolveHost(host);
    if (ip == 0 || !m_socket.connect(ip, server.port)) {
        log::info("chat: {} does not answer", host);
        m_state = State::Failed;
        return;
    }
    m_socket.send(std::format("CRYPT des 1 {}\r\n", gameName));
    m_state = State::Crypting;
}

void LobbyChat::stop() {
    if (m_state == State::Joined || m_state == State::Joining || m_state == State::Registering) {
        sendLine("QUIT :");
        m_socket.poll();
    }
    m_socket.close();
    m_state = State::Idle;
}

void LobbyChat::sendLine(const std::string& line) {
    std::string bytes = line + "\r\n";
    m_out.run(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    m_socket.send(bytes);
}

void LobbyChat::add(std::u16string nick, std::u16string text) {
    m_lines.push_back({std::move(nick), std::move(text)});
    while (m_lines.size() > kMostLines) m_lines.pop_front();
    ++m_unread;
}

void LobbyChat::frame() {
    if (m_state == State::Idle || m_state == State::Failed) return;
    m_socket.poll();
    if (m_socket.state() == TcpConnection::State::Failed || m_socket.lost()) {
        log::info("chat: the connection went");
        m_socket.close();
        m_state = State::Failed;
        return;
    }
    std::vector<uint8_t>& in = m_socket.received();
    if (in.empty()) return;
    std::string bytes(in.begin(), in.end());
    in.clear();
    if (m_state == State::Crypting) {
        // The server's challenges come plain: ":s 705 * <client's> <server's>"; what follows is ciphered.
        m_plain += bytes;
        const size_t end = m_plain.find('\n');
        if (end == std::string::npos) return;
        std::string line = m_plain.substr(0, end);
        bytes = m_plain.substr(end + 1);
        m_plain.clear();
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> w = words(line);
        if (w.size() < 5 || w[1] != "705") {
            log::info("chat: no cipher ({})", line);
            m_socket.close();
            m_state = State::Failed;
            return;
        }
        m_out.start(w[w.size() - 2], m_secretKey);
        m_in.start(w[w.size() - 1], m_secretKey);
        sendLine("USER XaaaaaaaaX|0 127.0.0.1 peerchat :" + m_nick);
        sendLine("NICK " + m_nick);
        m_state = State::Registering;
    }
    m_in.run(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    m_input += bytes;
    for (size_t end = m_input.find('\n'); end != std::string::npos; end = m_input.find('\n')) {
        std::string line = m_input.substr(0, end);
        m_input.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) serverLine(line);
    }
}

void LobbyChat::serverLine(const std::string& line) {
    const std::vector<std::string> w = words(line);
    if (w.empty()) return;
    if (w[0] == "PING") {
        sendLine("PONG :" + (w.size() > 1 ? w[1] : std::string()));
        return;
    }
    if (w.size() < 2) return;
    const std::string& command = w[1];
    if (command == "001") {
        if (w.size() > 2) m_nick = w[2];
        sendLine("JOIN " + m_channel);
        m_state = State::Joining;
    } else if (command == "433" || command == "432") {
        // The nick taken (or refused): a digit more, then another.
        if (++m_nickTries > 9) {
            m_state = State::Failed;
            return;
        }
        std::string base = m_nick.substr(0, std::min<size_t>(m_nick.size(), 18));
        if (m_nickTries > 1 && !base.empty()) base.pop_back();
        m_nick = base + static_cast<char>('0' + m_nickTries);
        sendLine("NICK " + m_nick);
    } else if (command == "366" && m_state == State::Joining) {
        m_state = State::Joined;
    } else if (command == "JOIN" && w.size() > 2) {
        const std::string who = prefixNick(w[0]);
        if (sameText(who, m_nick)) {
            m_state = m_state == State::Joining ? State::Joined : m_state;
        } else {
            add(u"", fromUtf8(who) + u" joined");
        }
    } else if ((command == "PART" || command == "QUIT") && w.size() > 1) {
        const std::string who = prefixNick(w[0]);
        if (!sameText(who, m_nick)) add(u"", fromUtf8(who) + u" left");
    } else if (command == "NICK" && w.size() > 2) {
        const std::string who = prefixNick(w[0]);
        if (sameText(who, m_nick)) {
            m_nick = w[2];
        } else {
            add(u"", fromUtf8(who) + u" is now " + fromUtf8(w[2]));
        }
    } else if (command == "PRIVMSG" && w.size() > 3 && sameText(w[2], m_channel)) {
        add(fromUtf8(prefixNick(w[0])), fromUtf8(w[3]));
    }
}

bool LobbyChat::say(std::u16string_view text) {
    if (m_state != State::Joined) return false;
    std::string bytes = gp::toUtf8(text);
    std::replace(bytes.begin(), bytes.end(), '\r', ' ');
    std::replace(bytes.begin(), bytes.end(), '\n', ' ');
    if (bytes.size() > 200) bytes.resize(200);
    if (bytes.empty()) return false;
    sendLine("PRIVMSG " + m_channel + " :" + bytes);
    m_lines.push_back({fromUtf8(m_nick), std::u16string(text)});
    while (m_lines.size() > kMostLines) m_lines.pop_front();
    return true;
}

namespace {

std::unique_ptr<LobbyChat> g_chat;
float g_retry = 0.0f;  // the time since the chat failed: it starts again after 15 s

}  // namespace

LobbyChat* lobbyChat() {
    return g_chat.get();
}

LobbyChat::Server& chatServer() {
    static LobbyChat::Server server;
    return server;
}

void frame(float dt) {
    // On in an online session with a session id and a user object (an account or a guest).
    NetGame* game = netGame();
    Matchmaking* mm = matchmaking();
    OnlineUser* user = onlineUser();
    const bool on = game != nullptr && mm != nullptr && mm->sessionId() != 0 && user != nullptr &&
                    user->loggedOn != 0 && !user->localPlayers.empty();
    if (!on) {
        end();
        return;
    }
    const std::string channel = sessionChannel(kGameName, mm->sessionId());
    if (g_chat && g_chat->state() == LobbyChat::State::Failed) {
        g_retry += dt;
        if (g_retry < 15.0f) return;
    }
    if (!g_chat || !sameText(g_chat->channel(), channel) || g_chat->state() == LobbyChat::State::Failed) {
        if (!g_chat) g_chat = std::make_unique<LobbyChat>();
        g_retry = 0.0f;
        g_chat->start(user->playerNick(user->localPlayers[0]), channel, kGameName, kSecretKey, chatServer());
    }
    g_chat->frame();
}

void end() {
    if (!g_chat) return;
    g_chat->stop();
    g_chat.reset();
}

}  // namespace oj::net::peerchat
