#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/NetTcp.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

namespace oj::net {

uint32_t resolveHost(const std::string& name) {
    // As the SDK: a dotted address as it is, else the name's first address.
    if (const unsigned long a = inet_addr(name.c_str()); a != INADDR_NONE) return a;
    const hostent* h = gethostbyname(name.c_str());
    if (h == nullptr || h->h_addr_list[0] == nullptr || h->h_length != 4) return 0;
    uint32_t ip = 0;
    std::memcpy(&ip, h->h_addr_list[0], 4);
    return ip;
}

TcpConnection::~TcpConnection() {
    close();
}

bool TcpConnection::connect(uint32_t ip, uint16_t port) {
    return open() && connectTo(ip, port);
}

bool TcpConnection::open() {
    close();
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        m_state = State::Failed;
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    m_socket = static_cast<uintptr_t>(s);
    m_lost = false;
    m_readError = false;
    m_in.clear();
    m_out.clear();
    return true;
}

void TcpConnection::tuneBuffers(std::initializer_list<int> receive, std::initializer_list<int> send) {
    if (m_socket == ~uintptr_t{0}) return;
    const auto s = static_cast<SOCKET>(m_socket);
    for (int size : receive) setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&size), sizeof size);
    for (int size : send) setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&size), sizeof size);
    int size = 0;
    int length = sizeof size;
    getsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&size), &length);
    length = sizeof size;
    getsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&size), &length);
}

bool TcpConnection::connectTo(uint32_t ip, uint16_t port) {
    if (m_socket == ~uintptr_t{0}) return false;
    const auto s = static_cast<SOCKET>(m_socket);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = ip;
    if (::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) == 0) {
        m_state = State::Connected;
        return true;
    }
    const int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
        close();
        m_state = State::Failed;
        return false;
    }
    m_state = State::Connecting;
    return true;
}

bool TcpConnection::connectBlocking(uint32_t ip, uint16_t port) {
    close();
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        m_state = State::Failed;
        return false;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = ip;
    m_lost = false;
    m_readError = false;
    m_in.clear();
    m_out.clear();
    if (::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0) {
        closesocket(s);
        m_state = State::Failed;
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    m_socket = static_cast<uintptr_t>(s);
    m_state = State::Connected;
    return true;
}

void TcpConnection::close(bool shutdownFirst) {
    if (m_socket != ~uintptr_t{0}) {
        if (shutdownFirst) shutdown(static_cast<SOCKET>(m_socket), SD_BOTH);
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~uintptr_t{0};
    }
    m_state = State::Closed;
}

void TcpConnection::send(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    m_out.insert(m_out.end(), p, p + size);
    if (m_state == State::Connected) flush();
}

void TcpConnection::flush() {
    while (!m_out.empty() && m_state == State::Connected) {
        const int sent = ::send(static_cast<SOCKET>(m_socket), reinterpret_cast<const char*>(m_out.data()),
                                static_cast<int>(m_out.size()), 0);
        if (sent > 0) {
            m_out.erase(m_out.begin(), m_out.begin() + sent);
            continue;
        }
        if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return;
        m_lost = true;
        m_state = State::Failed;
        return;
    }
}

void TcpConnection::poll() {
    if (m_socket == ~uintptr_t{0}) return;
    const auto s = static_cast<SOCKET>(m_socket);
    if (m_state == State::Connecting) {
        fd_set writable;
        fd_set failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        timeval none{};
        if (select(0, nullptr, &writable, &failed, &none) > 0) {
            if (FD_ISSET(s, &failed)) {
                m_state = State::Failed;
                return;
            }
            if (FD_ISSET(s, &writable)) m_state = State::Connected;
        }
        if (m_state != State::Connected) return;
    }
    if (m_state != State::Connected) return;
    flush();
    char buffer[4096];
    for (;;) {
        const int got = recv(s, buffer, sizeof buffer, 0);
        if (got > 0) {
            m_in.insert(m_in.end(), buffer, buffer + got);
            continue;
        }
        if (got == 0) {
            m_lost = true;
            m_state = State::Failed;
        } else if (WSAGetLastError() != WSAEWOULDBLOCK) {
            m_lost = true;
            m_readError = true;
            m_state = State::Failed;
        }
        return;
    }
}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open() {
    close();
    const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    if (bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0) {
        closesocket(s);
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    m_socket = static_cast<uintptr_t>(s);
    return true;
}

void UdpSocket::close() {
    if (m_socket != ~uintptr_t{0}) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~uintptr_t{0};
    }
}

uint16_t UdpSocket::localPort() const {
    if (m_socket == ~uintptr_t{0}) return 0;
    sockaddr_in a{};
    int size = sizeof a;
    getsockname(static_cast<SOCKET>(m_socket), reinterpret_cast<sockaddr*>(&a), &size);
    return a.sin_port;
}

bool UdpSocket::sendTo(uint32_t ip, uint16_t port, const void* data, size_t size) {
    if (m_socket == ~uintptr_t{0}) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = ip;
    a.sin_port = port;
    return sendto(static_cast<SOCKET>(m_socket), static_cast<const char*>(data), static_cast<int>(size), 0,
                  reinterpret_cast<const sockaddr*>(&a), sizeof a) != SOCKET_ERROR;
}

int UdpSocket::receive(void* data, size_t size, uint32_t& ip, uint16_t& port) {
    if (m_socket == ~uintptr_t{0}) return -1;
    sockaddr_in a{};
    int length = sizeof a;
    const int got = recvfrom(static_cast<SOCKET>(m_socket), static_cast<char*>(data), static_cast<int>(size), 0,
                             reinterpret_cast<sockaddr*>(&a), &length);
    if (got == SOCKET_ERROR) return -1;
    ip = a.sin_addr.s_addr;
    port = a.sin_port;
    return got;
}

TcpListener::~TcpListener() {
    close();
}

bool TcpListener::listen(uint16_t port, bool anyAddress, int backlog) {
    close();
    m_error = "";
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        m_error = "socket";
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = anyAddress ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    const char* failed = nullptr;
    int size = sizeof a;
    if (bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0) {
        failed = "bind";
    } else if (::listen(s, backlog) != 0) {
        failed = "listen";
    } else if (getsockname(s, reinterpret_cast<sockaddr*>(&a), &size) != 0) {
        failed = "getsockname";
    }
    if (failed != nullptr) {
        m_error = failed;
        closesocket(s);
        return false;
    }
    m_port = ntohs(a.sin_port);
    m_socket = static_cast<uintptr_t>(s);
    return true;
}

bool TcpListener::accept(TcpConnection& out) {
    if (m_socket == ~uintptr_t{0}) return false;
    const SOCKET s = ::accept(static_cast<SOCKET>(m_socket), nullptr, nullptr);
    if (s == INVALID_SOCKET) return false;
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    out.close();
    out.m_socket = static_cast<uintptr_t>(s);
    out.m_state = TcpConnection::State::Connected;
    out.m_lost = false;
    out.m_readError = false;
    out.m_in.clear();
    out.m_out.clear();
    return true;
}

void TcpListener::close() {
    if (m_socket != ~uintptr_t{0}) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~uintptr_t{0};
    }
}

}  // namespace oj::net
