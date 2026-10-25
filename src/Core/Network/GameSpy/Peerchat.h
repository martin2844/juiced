#pragma once
// The fixed build's chat of an online session's lobby (the user, 2026-10-24: as FlatOut 2's on GameSpy / OpenSpy; the
// stock game has no chat but the voice). GameSpy Peerchat: IRC at peerchat.<domain>:6667 whose bytes, after "CRYPT des
// 1 <game>" and the server's 705 with two challenges, go through a cipher each way; the session's players meet in the
// channel #GSP!<game>!<the matchmaking's session id>. Nothing of the stock executable: Peerchat's SDK was not linked.

#include "Core/Network/NetTcp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace oj::net::peerchat {

// The cipher: RC4 whose table begins 255 .. 0, keyed by the 16 bytes of a challenge each XORed with the game's
// secret key's byte in turn (the key over again after its end).
class Cipher {
public:
    void start(std::string_view challenge, std::string_view secretKey);
    void run(uint8_t* data, size_t size);

private:
    std::array<uint8_t, 256> m_table{};
    uint8_t m_i = 0;
    uint8_t m_j = 0;
};

// An IRC nick of the player's: the characters Peerchat takes ('A' .. '}', the digits, '-'), any other '_', not
// beginning with a digit or '-' ('J' before), 20 at most; "Juiced" for an empty one.
std::string chatNick(std::u16string_view nick);
// The session's channel: "#GSP!<game>!<session id, 8 hex digits>".
std::string sessionChannel(std::string_view gameName, uint32_t sessionId);

struct ChatLine {
    std::u16string nick;  // empty: a line of the chat's own (someone came or went)
    std::u16string text;
};

class LobbyChat {
public:
    enum class State { Idle, Connecting, Crypting, Registering, Joining, Joined, Failed };

    struct Server {
        std::string host;  // "" → "peerchat.<domain>"
        uint16_t port;
        Server() : port(6667) {}
        Server(std::string h, uint16_t p) : host(std::move(h)), port(p) {}
    };

    // Connects and asks for the cipher; the nick (chatNick's) and the channel taken once registered.
    void start(std::u16string_view nick, const std::string& channel, const std::string& gameName,
               const std::string& secretKey, const Server& server = {});
    // QUIT, the connection closed, the lines kept until the next start.
    void stop();
    // Once a frame: the bytes come, the server's lines (705, 001, 433, PING, JOIN, PART, QUIT, NICK, PRIVMSG).
    void frame();
    // A line of ours to the channel (UTF-8, cut to 200 bytes, no line feeds); shown at once. False when not joined.
    bool say(std::u16string_view text);

    State state() const { return m_state; }
    const std::string& channel() const { return m_channel; }
    const std::string& nick() const { return m_nick; }
    const std::deque<ChatLine>& lines() const { return m_lines; }
    // The lines come since unread() was last cleared.
    uint32_t unread() const { return m_unread; }
    void clearUnread() { m_unread = 0; }

    static constexpr size_t kMostLines = 50;

private:
    void sendLine(const std::string& line);
    void serverLine(const std::string& line);
    void add(std::u16string nick, std::u16string text);

    TcpConnection m_socket;
    State m_state = State::Idle;
    Cipher m_in;
    Cipher m_out;
    std::string m_plain;  // the bytes before the cipher's challenges came
    std::string m_input;  // deciphered, up to a whole line
    std::string m_nick;
    std::string m_channel;
    std::string m_gameName;
    std::string m_secretKey;
    int m_nickTries = 0;
    std::deque<ChatLine> m_lines;
    uint32_t m_unread = 0;
};

// The game's chat: on while an online session (the matchmaking, its session id) has a user object, its channel the
// session's, its nick our first player's (a failed one started again after 15 s); off otherwise (frame: once a frame,
// after the matchmaking's).
LobbyChat* lobbyChat();
void frame(float dt);
void end();
// For the tests: the server the game's chat reaches (the default: Peerchat on the services' domain).
LobbyChat::Server& chatServer();

}  // namespace oj::net::peerchat
