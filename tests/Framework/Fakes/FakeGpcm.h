#pragma once
// A stand-in of GameSpy's connection manager for the presence tests.

#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/NetTcp.h"

#include <windows.h>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace oj::test {

// A stand-in of GameSpy's connection manager on the loopback for the tests only (the game itself reaches
// gpcm.gamespy.com), on a thread of its own: the login's profile request waits inside the client's frame. Each
// connection gets the challenge (\lc\1); \login\ is answered with \lc\2 and the proof when its response is right, else
// the error 260 (fatal); \newuser\ with \nur\; \getprofile\ with \pi\ of the nick given for the profile. The messages
// a test queues are sent as they come.
class FakeGpcm {
public:
    std::string password = "secret";
    std::string serverChallenge = "QRSTUVWXYZ";
    int32_t sessKey = 4242;
    int32_t userId = 5678;
    int32_t profileId = 1234;
    bool badProof = false;
    std::map<int32_t, std::string> nicks;

    ~FakeGpcm() { stop(); }
    bool start() {
        if (!m_listener.listen(0)) return false;
        m_thread = std::thread([this] { run(); });
        return true;
    }
    void stop() {
        m_stop = true;
        if (m_thread.joinable()) m_thread.join();
        m_listener.close();
    }
    uint16_t port() const { return m_listener.port(); }
    void queue(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queued.push_back(message);
    }
    std::vector<std::string> messages() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages;
    }
    // The first message received beginning so; empty when none.
    std::string message(const std::string& start) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const std::string& m : m_messages) {
            if (m.rfind(start, 0) == 0) return m;
        }
        return {};
    }
    bool loginChecked() const { return m_loginChecked; }

private:
    static std::string value(const std::string& m, const std::string& key) {
        size_t at = m.find(key);
        if (at == std::string::npos) return {};
        at += key.size();
        const size_t end = m.find('\\', at);
        return m.substr(at, end == std::string::npos ? std::string::npos : end - at);
    }
    void answer(oj::net::TcpConnection& c, const std::string& m) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_messages.push_back(m);
        }
        namespace gp = oj::net::gp;
        if (m.rfind("\\login\\", 0) == 0) {
            const std::string client = value(m, "\\challenge\\");
            const std::string user = value(m, "\\user\\");
            const std::string hash = gp::md5Hex(password);
            if (value(m, "\\response\\") != gp::loginHash(hash, user, client, serverChallenge)) {
                c.send(std::string("\\error\\\\err\\260\\fatal\\\\errmsg\\The password is incorrect.\\id\\1\\final\\"));
                return;
            }
            m_loginChecked = true;
            const std::string proof = badProof ? std::string(32, '0') : gp::loginHash(hash, user, serverChallenge, client);
            c.send(std::format("\\lc\\2\\sesskey\\{}\\userid\\{}\\profileid\\{}\\uniquenick\\\\lt\\ticket\\proof\\{}"
                               "\\id\\1\\final\\",
                               sessKey, userId, profileId, proof));
        } else if (m.rfind("\\newuser\\", 0) == 0) {
            c.send(std::format("\\nur\\\\userid\\{}\\profileid\\{}\\id\\1\\final\\", userId, profileId));
        } else if (m.rfind("\\getprofile\\", 0) == 0) {
            const int32_t profile = std::atoi(value(m, "\\profileid\\").c_str());
            std::string email = nicks[profile] + "@example.com";
            for (char& ch : email) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            c.send(std::format("\\pi\\\\profileid\\{}\\nick\\{}\\uniquenick\\\\email\\{}\\sex\\1"
                               "\\sig\\0123456789abcdef0123456789abcdef\\id\\{}\\final\\",
                               profile, nicks[profile], email, value(m, "\\id\\")));
        }
    }
    void run() {
        oj::net::TcpConnection c;
        bool connected = false;
        std::string in;
        while (!m_stop) {
            if (!connected && m_listener.accept(c)) {
                connected = true;
                in.clear();
                c.send("\\lc\\1\\challenge\\" + serverChallenge + "\\id\\1\\final\\");
            }
            if (connected) {
                c.poll();
                std::vector<uint8_t>& r = c.received();
                in.append(r.begin(), r.end());
                r.clear();
                for (size_t f = in.find("\\final\\"); f != std::string::npos; f = in.find("\\final\\")) {
                    const std::string m = in.substr(0, f);
                    in.erase(0, f + 7);
                    answer(c, m);
                }
                std::vector<std::string> out;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    out.swap(m_queued);
                }
                for (const std::string& m : out) c.send(m);
                if (c.lost()) {
                    connected = false;
                    c.close();
                }
            }
            Sleep(1);
        }
    }

    oj::net::TcpListener m_listener;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_loginChecked{false};
    std::mutex m_mutex;
    std::vector<std::string> m_queued;
    std::vector<std::string> m_messages;
};

}  // namespace oj::test
