// Integration tests: Leaderboard.

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpyHttp.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"
#include "Core/Network/OnlineStats.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The leaderboards over ghttp against a stand-in server: a table's URL, its columns read (numbers, names,
// 64-bit numbers), the total; a table not read: the problem 3.
OJ_INTEGRATION_TEST(Leaderboards, Tables) {
    oj::net::startWinsock();
    oj::net::TcpListener listener;
    bool ok = listener.listen(0);
    oj::net::stats::Leaderboards lb;
    lb.host() = "127.0.0.1:" + std::to_string(listener.port());
    int32_t total = 0;
    int32_t done = 0;
    std::vector<int32_t> ranks;
    std::vector<const char16_t*> names;
    std::vector<int64_t> scores;
    ok = ok && lb.query(5, 10, -1, &total) && lb.bindInt32s(1, &ranks) && lb.bindNames(2, &names) &&
         lb.bindInt64s(3, &scores) && lb.endQuery() &&
         lb.url() == "http://127.0.0.1:" + std::to_string(listener.port()) + "/juicedpc/leaderboard_5.html";
    static int successes = 0;
    lb.request(&done, [](void*) { ++successes; }, nullptr, nullptr, nullptr);
    std::unique_ptr<oj::net::TcpConnection> server;
    std::string request;
    for (int i = 0; ok && i < 4000 && done == 0; ++i) {
        lb.think();
        if (server == nullptr) {
            auto c = std::make_unique<oj::net::TcpConnection>();
            if (listener.accept(*c)) server = std::move(c);
        }
        if (server != nullptr) {
            server->poll();
            std::vector<uint8_t>& got = server->received();
            request.append(got.begin(), got.end());
            got.clear();
            if (request.find("\r\n\r\n") != std::string::npos) {
                const std::string body = "3\n100\n1\nAlice\n9000000000\n2\nBob\n40\n3\nCarol\n30\n";
                server->send("HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
                             body);
                server->poll();
                server.reset();
            }
        }
        Sleep(1);
    }
    lb.think();  // done: ghttp cleaned up
    const bool tableOk = ok && done == 1 && successes == 1 && lb.read() && !lb.pending() && total == 100 &&
                         ranks == std::vector<int32_t>{1, 2, 3} && names.size() == 3 &&
                         std::u16string(names[0]) == u"Alice" && std::u16string(names[2]) == u"Carol" &&
                         scores == std::vector<int64_t>{9000000000LL, 40, 30} &&
                         oj::net::stats::problem() == 0 && oj::net::ghttp::references() == 0;
    // A port nothing listens on: the failure.
    listener.close();
    done = 0;
    bool failedOk = lb.query(5, 1, -1, nullptr) && lb.endQuery() &&
                    lb.url().ends_with("/juicedpc/records.asp?v=2&leaderboard=5");
    lb.request(&done, nullptr, nullptr, nullptr, nullptr);
    for (int i = 0; failedOk && i < 4000 && done == 0; ++i) {
        lb.think();
        Sleep(1);
    }
    lb.think();
    failedOk = failedOk && done == 1 && oj::net::stats::problem() == 3 && oj::net::ghttp::references() == 0;
    check(tableOk && failedOk, "statistics' leaderboards: a table's URL and columns, a table not read");
}

// OpenSpy's answer of every leaderboard ("1", "0", "0", "0", "CHC-Juiced"): a count of 1, a total of 0, then a rank,
// a name and a score; the car's column of top showoff scores gets nothing (the fixed build's table reader takes no row
// of it, OnlineRecords.ShortTableHasNoRow).
OJ_INTEGRATION_TEST(Leaderboards, OpenSpyAnswer) {
    oj::net::startWinsock();
    oj::net::TcpListener listener;
    bool ok = listener.listen(0);
    oj::net::stats::Leaderboards lb;
    lb.host() = "127.0.0.1:" + std::to_string(listener.port());
    int32_t total = -1;
    int32_t done = 0;
    std::vector<int32_t> ranks, scores, cars;
    std::vector<const char16_t*> names;
    ok = ok && lb.query(11, 10, -1, &total) && lb.bindInt32s(1, &ranks) && lb.bindNames(2, &names) &&
         lb.bindInt32s(15, &scores) && lb.bindInt32s(16, &cars) && lb.endQuery();
    lb.request(&done, nullptr, nullptr, nullptr, nullptr);
    std::unique_ptr<oj::net::TcpConnection> server;
    std::string request;
    for (int i = 0; ok && i < 4000 && done == 0; ++i) {
        lb.think();
        if (server == nullptr) {
            auto c = std::make_unique<oj::net::TcpConnection>();
            if (listener.accept(*c)) server = std::move(c);
        }
        if (server != nullptr) {
            server->poll();
            std::vector<uint8_t>& got = server->received();
            request.append(got.begin(), got.end());
            got.clear();
            if (request.find("\r\n\r\n") != std::string::npos) {
                const std::string body = "1\n0\n0\n0\nCHC-Juiced\n";
                server->send("HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) +
                             "\r\nConnection: keep-alive\r\n\r\n" + body);
                server->poll();
                server.reset();
            }
        }
        Sleep(1);
    }
    lb.think();
    check(ok && done == 1 && total == 0 && ranks.size() == 1 && names.size() == 1 && scores.size() == 1 &&
              cars.empty(),
          "statistics' leaderboards: OpenSpy's answer, a rank, a name, a score and no car");
}
