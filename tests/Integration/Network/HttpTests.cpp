// Integration tests: Http.

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpyHttp.h"
#include "Core/Network/GameSpy/GameSpyVoice.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// GameSpy's HTTP (ghttp) against a stand-in server on the loopback: the request's text, a body by its
// length, a chunked one, a 404's result, a redirect to a path of the server, a URL not "http://".
OJ_INTEGRATION_TEST(GameSpyHttp, Requests) {
    struct Got {
        int calls = 0;
        int32_t result = -1;
        std::string body;
        int64_t length = -1;
    };
    const oj::net::ghttp::CompletedCallback done = [](int32_t, int32_t result, char* buffer, int64_t length,
                                                      void* param) {
        Got& g = *static_cast<Got*>(param);
        ++g.calls;
        g.result = result;
        g.length = length;
        if (buffer != nullptr) g.body.assign(buffer, static_cast<size_t>(length));
        return true;
    };
    oj::net::startWinsock();
    oj::net::TcpListener listener;
    bool ok = listener.listen(0);
    const std::string base = "http://127.0.0.1:" + std::to_string(listener.port());
    std::vector<std::string> requests;
    const auto run = [&](const std::string& url, const std::vector<std::string>& answers, Got& g) {
        g = Got{};
        const int32_t id = oj::net::ghttp::get(url.c_str(), false, done, &g);
        std::unique_ptr<oj::net::TcpConnection> server;
        std::string request;
        size_t next = 0;
        for (int i = 0; ok && i < 4000 && g.calls == 0; ++i) {
            oj::net::ghttp::think();
            if (server == nullptr && next < answers.size()) {
                auto c = std::make_unique<oj::net::TcpConnection>();
                if (listener.accept(*c)) {
                    server = std::move(c);
                    request.clear();
                }
            }
            if (server != nullptr) {
                server->poll();
                std::vector<uint8_t>& in = server->received();
                request.append(in.begin(), in.end());
                in.clear();
                if (request.find("\r\n\r\n") != std::string::npos) {
                    requests.push_back(request);
                    server->send(answers[next++]);
                    server->poll();
                    server.reset();
                }
            }
            Sleep(1);
        }
        return id >= 0 && g.calls == 1;
    };
    Got plain, chunked, missing, moved, bad;
    const bool plainOk =
        run(base + "/x y?z=1", {"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello"}, plain) &&
        plain.result == oj::net::ghttp::kSuccess && plain.body == "hello" && requests.size() == 1 &&
        requests[0] == "GET /x+y?z=1 HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(listener.port()) +
                           "\r\nUser-Agent: GameSpyHTTP/1.0\r\nConnection: close\r\n\r\n";
    const bool chunkedOk =
        run(base + "/c",
            {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n"},
            chunked) &&
        chunked.result == oj::net::ghttp::kSuccess && chunked.body == "hello world";
    const bool missingOk =
        run(base + "/m", {"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"}, missing) &&
        missing.result == oj::net::ghttp::kFileNotFound && missing.length == 0;
    const size_t before = requests.size();
    const bool movedOk = run(base + "/old",
                             {"HTTP/1.1 302 Found\r\nLocation: /new\r\n\r\n",
                              "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"},
                             moved) &&
                         moved.result == oj::net::ghttp::kSuccess && moved.body == "ok" &&
                         requests.size() == before + 2 && requests[before + 1].starts_with("GET /new HTTP/1.1");
    const bool badOk = oj::net::ghttp::get("", false, done, &bad) == -1 &&
                       run("ftp://127.0.0.1/", {}, bad) && bad.result == oj::net::ghttp::kParseUrlFailed;
    // The fixed build: a Content-Length with no space after its colon (the stock read "" past the digit, a length of
    // 0: done with no body) and headers ended by "\n\n" (the stock's body lost its first two bytes).
    Got tight, bare;
    const bool tightOk = run(base + "/t", {"HTTP/1.1 200 OK\r\nContent-Length:5\r\n\r\nhello"}, tight) &&
                         tight.result == oj::net::ghttp::kSuccess && tight.body == "hello";
    const bool bareOk = run(base + "/b", {"HTTP/1.1 200 OK\r\nContent-Length: 5\n\nhello"}, bare) &&
                        bare.result == oj::net::ghttp::kSuccess && bare.body == "hello";
    oj::net::ghttp::cleanup();
    check(ok && plainOk && chunkedOk && missingOk && movedOk && badOk &&
              oj::net::ghttp::references() == 0,
          "GameSpy HTTP: the request, a body by length, chunked, a 404, a redirect, a bad URL");
    check(tightOk && bareOk, "GameSpy HTTP: Content-Length without a space, headers ended by \\n\\n");
}
