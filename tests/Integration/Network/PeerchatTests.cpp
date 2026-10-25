// Integration tests: Peerchat (the fixed build's lobby chat) against a stand-in server on the loopback.

#include "Framework/Test.h"

#include "Core/Network/GameSpy/Peerchat.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;
using oj::net::peerchat::Cipher;
using oj::net::peerchat::LobbyChat;

// The server's side: the plain CRYPT, the challenges, then each way through its cipher; the client's lines kept.
struct StandIn {
    oj::net::TcpListener listener;
    std::unique_ptr<oj::net::TcpConnection> c;
    Cipher in, out;
    bool crypted = false;
    std::string plain, input;
    std::vector<std::string> lines;

    void send(const std::string& line) {
        std::string bytes = line + "\r\n";
        out.run(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
        c->send(bytes);
    }
    void frame() {
        if (c == nullptr) {
            auto n = std::make_unique<oj::net::TcpConnection>();
            if (listener.accept(*n)) c = std::move(n);
            return;
        }
        c->poll();
        std::vector<uint8_t>& got = c->received();
        std::string bytes(got.begin(), got.end());
        got.clear();
        if (!crypted) {
            plain += bytes;
            const size_t end = plain.find("\r\n");
            if (end == std::string::npos) return;
            lines.push_back(plain.substr(0, end));
            bytes = plain.substr(end + 2);
            c->send(":s 705 * AAAABBBBCCCCDDDD EEEEFFFFGGGGHHHH\r\n");
            in.start("AAAABBBBCCCCDDDD", "g3J8df2");
            out.start("EEEEFFFFGGGGHHHH", "g3J8df2");
            crypted = true;
        }
        in.run(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
        input += bytes;
        for (size_t end = input.find("\r\n"); end != std::string::npos; end = input.find("\r\n")) {
            const std::string line = input.substr(0, end);
            input.erase(0, end + 2);
            lines.push_back(line);
            if (line == "NICK Racer") {
                send(":s 433 * Racer :Nickname is already in use");
            } else if (line.starts_with("NICK ")) {
                send(":s 001 " + line.substr(5) + " :Welcome");
            } else if (line.starts_with("JOIN ")) {
                send(":Racer1!X@* JOIN " + line.substr(5));
                send(":s 366 Racer1 " + line.substr(5) + " :End of /NAMES list.");
                send(":Other!X@* JOIN " + line.substr(5));
                send(":Other!X@* PRIVMSG " + line.substr(5) + " :hello there");
                send("PING :123");
            }
        }
    }
    bool has(const std::string& line) const {
        for (const std::string& l : lines) {
            if (l == line) return true;
        }
        return false;
    }
};

}  // namespace

// The lobby chat's login (CRYPT, the challenges, USER and NICK, a nick taken and the next tried), the session's channel
// joined, another's coming and line, PING answered, a line of ours sent and shown, QUIT.
OJ_INTEGRATION_TEST(Peerchat, LobbyChat) {
    oj::net::startWinsock();
    StandIn server;
    bool ok = server.listener.listen(0);
    LobbyChat chat;
    const std::string channel = oj::net::peerchat::sessionChannel("juicedpc", 0xABCDu);
    if (ok) chat.start(u"Racer", channel, "juicedpc", "g3J8df2", {"127.0.0.1", server.listener.port()});
    for (int i = 0; ok && i < 3000 && !(chat.state() == LobbyChat::State::Joined && chat.lines().size() >= 2 &&
                                        server.has("PONG :123"));
         ++i) {
        server.frame();
        chat.frame();
        Sleep(1);
    }
    const bool joined = chat.state() == LobbyChat::State::Joined && chat.nick() == "Racer1" &&
                        server.has("CRYPT des 1 juicedpc") && server.has("JOIN " + channel) &&
                        server.has("PONG :123");
    const bool heard = chat.lines().size() >= 2 && chat.lines()[0].nick.empty() &&
                       chat.lines()[0].text == u"Other joined" && chat.lines()[1].nick == u"Other" &&
                       chat.lines()[1].text == u"hello there" && chat.unread() == 2;
    const bool said = chat.say(u"hi all") && chat.lines().back().nick == u"Racer1";
    for (int i = 0; ok && said && i < 1000 && !server.has("PRIVMSG " + channel + " :hi all"); ++i) {
        chat.frame();
        server.frame();
        Sleep(1);
    }
    const bool sent = server.has("PRIVMSG " + channel + " :hi all");
    chat.stop();
    for (int i = 0; ok && i < 500 && !server.has("QUIT :"); ++i) {
        server.frame();
        Sleep(1);
    }
    check(ok && joined, "peerchat: the cipher's login, a nick taken and the next, the session's channel, PING");
    check(heard && said && sent, "peerchat: another's coming and line, ours sent and shown");
}
