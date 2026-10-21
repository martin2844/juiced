// Integration tests: Tcp.

#include "Framework/Test.h"

#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <string>

namespace {

using oj::test::check;

}  // namespace

// The online services' TCP: a connection to a listener on the loopback, bytes both ways, the close seen.
OJ_INTEGRATION_TEST(OnlineTcp, Connection) {
    oj::net::startWinsock();
    oj::net::TcpListener listener;
    oj::net::TcpConnection client;
    oj::net::TcpConnection server;
    bool ok = listener.listen(0) && client.connect(0x0100007Fu, listener.port());
    bool accepted = false;
    for (int i = 0; ok && i < 2000 && !(accepted && client.connected()); ++i) {
        if (!accepted) accepted = listener.accept(server);
        client.poll();
        Sleep(0);
    }
    ok = ok && accepted && client.connected();
    const std::string sent = "\\lc\\1\\final\\";
    client.send(sent);
    server.send(std::string("hello"));
    for (int i = 0; ok && i < 2000 && (server.received().size() < sent.size() || client.received().size() < 5);
         ++i) {
        client.poll();
        server.poll();
        Sleep(0);
    }
    const bool both = std::string(server.received().begin(), server.received().end()) == sent &&
                      std::string(client.received().begin(), client.received().end()) == "hello";
    server.close();
    for (int i = 0; ok && i < 2000 && !client.lost(); ++i) {
        client.poll();
        Sleep(0);
    }
    check(ok && both && client.lost(), "online TCP: a connection, bytes both ways, the close seen");
}
