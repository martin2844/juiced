#pragma once
// The sockets of the online services (GameSpy's presence, server list, NAT negotiation): non-blocking, polled each
// frame from the SDK's think calls.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace oj::net {

// An address of IPv4 in network byte order (as sockaddr_in keeps it): a dotted address as it is (inet_addr), else
// the name's first (gethostbyname), as the SDK does; 0 when the name does not resolve.
uint32_t resolveHost(const std::string& name);

class TcpConnection {
public:
    enum class State { Closed, Connecting, Connected, Failed };

    TcpConnection() = default;
    ~TcpConnection();
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Begins a connection to an address (network order) and a port; false when no socket can be had.
    bool connect(uint32_t ip, uint16_t port);
    // connect() in two steps, for options set between them: a socket made (non-blocking), then its connection begun
    // (false when it fails at once).
    bool open();
    bool connectTo(uint32_t ip, uint16_t port);
    // Buffer sizes set in turn, each setsockopt on its own (one refused leaves the last taken), then both read back:
    // GameSpy Presence's peer sockets (0x0057B830).
    void tuneBuffers(std::initializer_list<int> receive, std::initializer_list<int> send);
    // Connects and waits for it as the SDK's connect() does (blocking), the socket non-blocking after; false when it
    // cannot (no socket, or the connection refused or timed out).
    bool connectBlocking(uint32_t ip, uint16_t port);
    // shutdownFirst: shutdown(SD_BOTH) before the socket is closed.
    void close(bool shutdownFirst = false);
    // The connection's progress (a pending connect finishing or failing) and the bytes come; the bytes waiting to be
    // sent are sent as far as the socket takes them. Call once a frame.
    void poll();

    // Queued and sent by poll() (and at once as far as possible once connected).
    void send(const void* data, size_t size);
    void send(const std::string& text) { send(text.data(), text.size()); }
    // The bytes received so far, taken from the front as they are read.
    std::vector<uint8_t>& received() { return m_in; }

    State state() const { return m_state; }
    bool connected() const { return m_state == State::Connected; }
    bool opened() const { return m_socket != ~uintptr_t{0}; }
    // The peer closed its side (read 0 bytes) or the connection broke after it was made.
    bool lost() const { return m_lost; }
    // Lost by an error of recv() (not a close).
    bool readError() const { return m_readError; }
    // The bytes queued that the socket has not taken yet.
    size_t pending() const { return m_out.size(); }

private:
    friend class TcpListener;
    void flush();

    uintptr_t m_socket = ~uintptr_t{0};
    State m_state = State::Closed;
    bool m_lost = false;
    bool m_readError = false;
    std::vector<uint8_t> m_out;
    std::vector<uint8_t> m_in;
};

// A UDP socket of the online services (the server browser's queries, NAT negotiation's own socket, the available
// check), non-blocking, bound to any free port.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open();  // false when no socket can be had
    void close();
    bool opened() const { return m_socket != ~uintptr_t{0}; }
    // The socket's own port (network order; getsockname).
    uint16_t localPort() const;
    // Network order address and port.
    bool sendTo(uint32_t ip, uint16_t port, const void* data, size_t size);
    // A datagram waiting (at most `size` bytes), its sender; -1 when none.
    int receive(void* data, size_t size, uint32_t& ip, uint16_t& port);

private:
    uintptr_t m_socket = ~uintptr_t{0};
};

// A listening socket, non-blocking: GameSpy Presence's for its peers (0x00573930) and the tests' stand-ins of the
// online services.
class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    // Listens on the port (0: any free one) of 127.0.0.1, or of every address, with the backlog given; false when it
    // cannot, error() naming the step: "socket", "bind", "listen" or "getsockname".
    bool listen(uint16_t port, bool anyAddress = false, int backlog = 4);
    const char* error() const { return m_error; }
    bool opened() const { return m_socket != ~uintptr_t{0}; }
    uint16_t port() const { return m_port; }
    // A connection come since the last call, made into `out` (connected); false when none.
    bool accept(TcpConnection& out);
    void close();

private:
    uintptr_t m_socket = ~uintptr_t{0};
    uint16_t m_port = 0;
    const char* m_error = "";
};

}  // namespace oj::net
