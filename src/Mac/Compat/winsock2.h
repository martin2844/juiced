#pragma once
#include "windows.h"
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
struct WSADATA {};
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_BOTH SHUT_RDWR
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAECONNRESET ECONNRESET
#define WSAENOTCONN ENOTCONN
#define WSAECONNREFUSED ECONNREFUSED
inline int WSAStartup(WORD, WSADATA *) {
    return 0;
}
inline void WSACleanup() {}
inline int WSAGetLastError() {
    return errno;
}
inline int closesocket(SOCKET s) {
    return close(s);
}
inline int ioctlsocket(SOCKET s, long cmd, unsigned long *v) {
    int value = *v;
    return ioctl(s, cmd, &value);
}
inline int recvfrom(int s, void *b, size_t n, int f, sockaddr *a, int *l) {
    socklen_t len = *l;
    int r = ::recvfrom(s, b, n, f, a, &len);
    *l = len;
    return r;
}
inline int getsockname(int s, sockaddr *a, int *l) {
    socklen_t len = *l;
    int r = ::getsockname(s, a, &len);
    *l = len;
    return r;
}
inline int getsockopt(int s, int level, int opt, void *b, int *l) {
    socklen_t len = *l;
    int r = ::getsockopt(s, level, opt, b, &len);
    *l = len;
    return r;
}
inline int winSelect(int n, fd_set *r, fd_set *w, fd_set *e, timeval *t) {
    if (n == 0)
        n = FD_SETSIZE;
    return ::select(n, r, w, e, t);
}
#define select winSelect

#define WSAETIMEDOUT ETIMEDOUT
