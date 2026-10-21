#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyHttp.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <format>
#include <mutex>

namespace oj::net::ghttp {

namespace {

// The SDK's state (ghttpMain.c's and ghttpConnection.c's statics).
struct Sdk {
    int32_t references = 0;                       // [0x0075B568]
    std::unique_ptr<std::recursive_mutex> lock;   // [0x0075B920]
    int32_t throttleBufferSize = 125;             // [0x0071B210]
    uint32_t throttleTime = 250;                  // [0x0071B214]
    std::vector<std::unique_ptr<Request>> requests;  // [0x0075B908], their count [0x0075B900]
    int32_t active = 0;                           // [0x0075B904]
    int32_t uniqueId = 0;                         // [0x0075B90C]
    std::string proxy;                            // [0x007A4338]
    uint16_t proxyPort = 0;                       // [0x007A4334]
};

Sdk& sdk() {
    static Sdk s;
    return s;
}

// The lock entered and left when it is there.
class Guard {
public:
    Guard() : m_lock(sdk().lock.get()) {
        if (m_lock != nullptr) m_lock->lock();
    }
    ~Guard() {
        if (m_lock != nullptr) m_lock->unlock();
    }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

private:
    std::recursive_mutex* m_lock;
};

constexpr const char* kBoundary = "--Qr4G823s23d---<<><><<<>--7d118e0536\r\n";
constexpr const char* kBoundaryAfter = "\r\n--Qr4G823s23d---<<><><<<>--7d118e0536\r\n";
constexpr const char* kLastBoundary = "\r\n--Qr4G823s23d---<<><><<<>--7d118e0536--\r\n";  // 0x006FB0DC, 0x2B bytes
constexpr const char* kMultipart = "multipart/form-data; boundary=Qr4G823s23d---<<><><<<>--7d118e0536";
constexpr const char* kUrlEncoded = "application/x-www-form-urlencoded";
// 0x006FB270: the characters a post's string keeps as they are.
constexpr const char* kLegalUrlChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_@-.*";
// 0x006FB1DC .. 0x006FB1FB: "0123456789ABCDEF" (0x006FB1EC) with the 16 bytes before it, which the encoding of a
// character over 0x7F reads (its signed nibbles -8 .. -1 and -15 .. -1).
constexpr char kHexWithBefore[32] = {'6', '\r', '\n', 0, '&', '%', 's', '=', 0, 0, 0, 0, '%', 's', '=', 0,
                                     '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

constexpr SOCKET socketOf(const Request& r) {
    return static_cast<SOCKET>(r.socket);
}

// The errors a non-blocking socket gives while it goes on: WSAEWOULDBLOCK, WSAEINPROGRESS, WSAETIMEDOUT.
bool softError(int error) {
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAETIMEDOUT;
}

// 0x0056E2C0: select without waiting on the asked sets; a failure tried again up to 20 times (the socket's error
// read between), -1 after the 20th try (even when it worked).
int selectSocket(SOCKET s, bool* readable, bool* writable, bool* failed) {
    fd_set readSet;
    fd_set writeSet;
    fd_set failSet;
    fd_set* reads = nullptr;
    fd_set* writes = nullptr;
    fd_set* fails = nullptr;
    if (readable != nullptr) {
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        reads = &readSet;
    }
    if (writable != nullptr) {
        FD_ZERO(&writeSet);
        FD_SET(s, &writeSet);
        writes = &writeSet;
    }
    if (failed != nullptr) {
        FD_ZERO(&failSet);
        FD_SET(s, &failSet);
        fails = &failSet;
    }
    timeval none{};
    int got = select(64, reads, writes, fails, &none);
    if (got == SOCKET_ERROR) {
        int tries = 0;
        do {
            if (tries > 19) break;
            char error = 0;
            int size = 1;
            getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &size);
            got = select(64, reads, writes, fails, &none);
            ++tries;
        } while (got == SOCKET_ERROR);
        if (tries == 20) return -1;
    }
    if (readable != nullptr) *readable = got > 0 && FD_ISSET(s, reads) != 0;
    if (writable != nullptr) *writable = got > 0 && FD_ISSET(s, writes) != 0;
    if (failed != nullptr) *failed = got > 0 && FD_ISSET(s, fails) != 0;
    return got;
}

void fail(Request& r, int32_t result) {
    r.completed = 1;
    r.result = result;
}

// 0x00571500: the progress callback with the request's state (the calls give no bytes).
void progress(Request& r) {
    if (r.progressCallback != nullptr) {
        r.progressCallback(r.request, r.state, nullptr, 0, r.fileBytesReceived, r.totalSize, r.callbackParam);
    }
}

// 0x00571550: a post's progress.
void postProgress(Request& r) {
    if (r.postCallback != nullptr) {
        r.postCallback(r.request, r.postBytesPosted, r.postTotalBytes, r.postStateIndex,
                       static_cast<int32_t>(r.postStates->size()), r.callbackParam);
    }
}

// 0x0056F3D0: a free slot, else four more (0x138 bytes each) and the first of them; -1 when none can be had.
int32_t freeSlot() {
    Sdk& s = sdk();
    for (size_t i = 0; i < s.requests.size(); ++i) {
        if (!s.requests[i]->inUse) return static_cast<int32_t>(i);
    }
    const auto first = static_cast<int32_t>(s.requests.size());
    for (int i = 0; i < 4; ++i) s.requests.push_back(std::make_unique<Request>());
    return first;
}

bool freeRequest(Request& r);

// 0x0056F480: a request in a free slot, its numbers and its send (0x1000 more at a time) and receive buffers.
Request* newRequest() {
    Guard guard;
    Sdk& s = sdk();
    const int32_t slot = freeSlot();
    if (slot == -1) return nullptr;
    Request& r = *s.requests[static_cast<size_t>(slot)];
    r = Request{};
    r.uniqueId = s.uniqueId++;
    r.inUse = true;
    r.request = slot;
    if (r.sendBuffer.init(&r, 0x1000) && r.recvBuffer.init(&r, 0x800)) {
        ++s.active;
        return &r;
    }
    freeRequest(r);  // the count goes down though it never went up
    return nullptr;
}

// 0x0056F8F0: the posting states' files closed and the states freed; an auto-freed post freed.
void freePostStates(Request& r) {
    if (r.postStates != nullptr) {
        for (PostState& state : *r.postStates) {
            if (state.data->type == 1) {
                if (state.file != nullptr) std::fclose(state.file);
                state.file = nullptr;
            }
        }
        r.postStates.reset();
    }
    if (r.post != nullptr && r.post->autoFree) {
        delete r.post;
        r.post = nullptr;
    }
}

// 0x0056F650: the request's strings, file, socket and buffers freed, its slot free again.
bool freeRequest(Request& r) {
    Sdk& s = sdk();
    if (!r.inUse || r.request < 0 || r.request >= static_cast<int32_t>(s.requests.size())) return false;
    Guard guard;
    r.url.clear();
    r.serverAddress.clear();
    r.requestPath.clear();
    r.sendHeaders.clear();
    r.redirectUrl.clear();
    r.proxyOverrideServer.clear();
    if (r.saveFile != nullptr) std::fclose(r.saveFile);
    if (r.socket != ~uintptr_t{0}) {
        shutdown(socketOf(r), SD_BOTH);
        closesocket(socketOf(r));
    }
    r.sendBuffer.free();
    r.recvBuffer.free();
    r.getFileBuffer.free();
    if (r.postStates != nullptr) freePostStates(r);
    if (r.post != nullptr && r.post->autoFree) {
        delete r.post;
        r.post = nullptr;
    }
    --s.active;
    r.inUse = false;
    return true;
}

// 0x0056F760: a function run on each request in use.
template <typename F>
void eachRequest(F f) {
    Sdk& s = sdk();
    if (s.active <= 0) return;
    Guard guard;
    for (size_t i = 0; i < s.requests.size(); ++i) {
        Request& r = *s.requests[i];
        if (r.inUse) f(r);
    }
}

// 0x0056F860: every request freed, the slots too.
void freeAll() {
    Sdk& s = sdk();
    if (s.requests.empty()) return;
    eachRequest([](Request& r) { freeRequest(r); });
    s.requests.clear();
    s.active = 0;
}

// 0x00570400: "http://<server>[:<port>]<path>": the server up to ':' or '/', the port (80 without one; 0 fails),
// the path from the next '/' after the port's first digit (none: ""), its spaces '+'.
bool parseUrl(Request& r) {
    const char* url = r.url.c_str();
    if (std::strncmp(url, "http://", 7) != 0) return false;
    const char* p = url + 7;
    const size_t serverLength = std::strcspn(p, ":/");
    r.serverAddress.assign(p, serverLength);
    p += serverLength;
    if (*p == ':') {
        ++p;
        r.serverPort = static_cast<uint16_t>(static_cast<int16_t>(std::atol(p)));
        if (r.serverPort == 0) return false;
        do {
            ++p;
        } while (*p != '\0' && *p != '/');
    } else {
        r.serverPort = 80;
    }
    r.requestPath = p;
    std::replace(r.requestPath.begin(), r.requestPath.end(), ' ', '+');
    return true;
}

// 0x005704E0: the progress, Winsock started (each time, as the SDK), the URL parsed, the address of the proxy or the
// server (dotted, else looked up).
void hostLookup(Request& r) {
    progress(r);
    WSADATA data;
    WSAStartup(0x101, &data);
    if (!parseUrl(r)) {
        fail(r, kParseUrlFailed);
        return;
    }
    const std::string& host = !r.proxyOverrideServer.empty() ? r.proxyOverrideServer
                              : !sdk().proxy.empty()          ? sdk().proxy
                                                              : r.serverAddress;
    r.serverIp = inet_addr(host.c_str());
    if (r.serverIp == INADDR_NONE) {
        const hostent* h = gethostbyname(host.c_str());
        if (h == nullptr) {
            fail(r, kHostLookupFailed);
            return;
        }
        std::memcpy(&r.serverIp, h->h_addr_list[0], 4);
    }
    r.state = kConnecting;
    progress(r);
}

// 0x00570620: the socket made non-blocking (the throttle's receive buffer), the connect begun; then the socket
// writable: on to the request.
void connecting(Request& r) {
    if (r.socket == ~uintptr_t{0}) {
        const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        r.socket = static_cast<uintptr_t>(s);
        bool made = false;
        if (s != INVALID_SOCKET) {
            u_long nonBlocking = 1;
            if (ioctlsocket(s, FIONBIO, &nonBlocking) == 0) {
                if (r.throttle) {
                    const int size = sdk().throttleBufferSize;
                    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&size), sizeof size);
                }
                sockaddr_in a{};
                a.sin_family = AF_INET;
                const uint16_t port = !r.proxyOverrideServer.empty() ? r.proxyOverridePort
                                      : !sdk().proxy.empty()          ? sdk().proxyPort
                                                                      : r.serverPort;
                a.sin_port = htons(port);
                a.sin_addr.s_addr = r.serverIp;
                if (connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) == SOCKET_ERROR) {
                    const int error = WSAGetLastError();
                    if (!softError(error)) {
                        fail(r, kConnectFailed);
                        r.socketError = error;
                        return;
                    }
                }
                made = true;
            }
        }
        if (!made) {
            fail(r, kSocketFailed);
            r.socketError = WSAGetLastError();
            return;
        }
    }
    bool writable = false;
    bool failed = false;
    // The select's value kept as its low byte and compared with -1 (Stock problem: never equal, a failed select
    // reads as one with nothing set).
    const int32_t selected = selectSocket(socketOf(r), nullptr, &writable, &failed) & 0xFF;
    if (!failed) {
        if (!writable) return;
        r.state = kSendingRequest;
        progress(r);
        return;
    }
    fail(r, kConnectFailed);
    if (selected != 0) return;
    r.socketError = WSAGetLastError();
}

// 0x005701C0: the send buffer's rest sent while the socket takes it; false on a failure.
bool sendBufferedData(Request& r) {
    Buffer& b = r.sendBuffer;
    for (;;) {
        bool writable = false;
        bool failed = false;
        if (selectSocket(socketOf(r), nullptr, &writable, &failed) == -1 || failed) {
            fail(r, kSocketFailed);
            r.socketError = WSAGetLastError();
            return false;
        }
        if (!writable) return true;
        int sent = send(socketOf(r), b.data + b.pos, b.len - b.pos, 0);
        if (sent == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (!softError(error)) {
                r.socketError = error;
                fail(r, kSocketFailed);
                return false;
            }
            sent = 0;
        } else if (r.state == kPosting) {
            r.postBytesPosted += sent;
        }
        b.pos += sent;
        if (b.len <= b.pos) return true;
    }
}

// 0x00570350: bytes sent at once: their count (0 when the socket takes none), -1 on a failure.
int32_t sendData(Request& r, const char* data, int32_t length) {
    const int sent = send(socketOf(r), data, length, 0);
    if (sent != SOCKET_ERROR) {
        if (r.state == kPosting) r.postBytesPosted += sent;
        return sent;
    }
    const int error = WSAGetLastError();
    if (!softError(error)) {
        r.socketError = error;
        fail(r, kSocketFailed);
        return -1;
    }
    return 0;
}

// 0x005703B0: sent at once when nothing waits in the send buffer, the rest buffered: 1 all sent, 2 buffered, 0 a
// failure.
int32_t sendOrBuffer(Request& r, const char* data, int32_t length) {
    int32_t sent = 0;
    if (r.sendBuffer.len == 0) {
        sent = sendData(r, data, length);
        if (sent == -1) return 0;
        if (sent == length) return 1;
    }
    return r.sendBuffer.add(data + sent, length - sent) ? 2 : 0;
}

// 0x005707B0: the request (its line, Host, User-Agent, Connection, a post's length and type, the caller's
// headers) put in the send buffer once and sent; all sent, on to the post or the answer.
void sendingRequest(Request& r) {
    Buffer& b = r.sendBuffer;
    if (b.len == 0) {
        b.add(r.post != nullptr ? "POST " : r.type == Type::Head ? "HEAD " : "GET ");
        b.add(r.proxyOverrideServer.empty() && sdk().proxy.empty() ? r.requestPath.c_str() : r.url.c_str());
        b.add(" HTTP/1.1\r\n");
        if (r.serverPort == 80) {
            b.addHeader("Host", r.serverAddress.c_str());
        } else {
            b.add("Host: ");
            b.add(r.serverAddress.c_str());
            b.addChar(':');
            b.addInt(r.serverPort);
            b.add("\r\n");
        }
        b.addHeader("User-Agent", "GameSpyHTTP/1.0");
        b.addHeader("Connection", "close");
        if (r.post != nullptr) {
            b.addHeader("Content-Length", std::to_string(r.postTotalBytes).c_str());
            b.addHeader("Content-Type", r.post->hasFiles ? kMultipart : kUrlEncoded);
        }
        if (!r.sendHeaders.empty()) b.add(r.sendHeaders.c_str());
        b.add("\r\n");
    }
    if (sendBufferedData(r) && b.len <= b.pos) {
        b.reset();
        r.state = r.post != nullptr ? kPosting : kWaiting;
        progress(r);
    }
}

// 0x0056F9C0: a string object: without files and with invalid characters URL-encoded into the send buffer (a space
// '+', else "%XX" of the signed character: Stock problem: one over 0x7F reads the bytes before the digits), else
// sent from its start (Stock problem: from the start again after a part was sent): 1 done, 2 not yet, 0 a failure.
int32_t postString(Request& r, PostState& state) {
    const PostData& d = *state.data;
    const int32_t length = d.len;
    if (length == 0) return 1;
    if (!r.post->hasFiles && d.invalidChars) {
        char encoded[4] = {'%', '0', '0', 0};
        for (const char* p = d.string.c_str(); *p != '\0'; ++p) {
            const int c = *p;
            if (std::strchr(kLegalUrlChars, c) != nullptr) {
                r.sendBuffer.addChar(static_cast<char>(c));
            } else if (c == ' ') {
                r.sendBuffer.addChar('+');
            } else {
                encoded[1] = kHexWithBefore[16 + c / 16];
                encoded[2] = kHexWithBefore[16 + c % 16];
                r.sendBuffer.add(encoded, 3);
            }
        }
        return 1;
    }
    const int32_t rest = length - state.pos;
    const int32_t sent = sendData(r, d.string.data(), rest);
    if (sent == -1) return 0;
    state.pos += sent;
    return sent != rest ? 2 : 1;
}

// 0x0056FAC0: a file on disk read 0x1000 bytes at a time, sent or buffered; a read short of its size or past it
// kFileReadFailed.
int32_t postFile(Request& r, PostState& state) {
    char data[0x1000];
    for (;;) {
        const auto read = static_cast<int32_t>(std::fread(data, 1, sizeof data, state.file));
        if (read < 1 || (state.pos += read, state.size < state.pos)) {
            r.result = kFileReadFailed;
            r.completed = 1;
            return 0;
        }
        int32_t sent = 0;
        int32_t how = 0;
        if (r.sendBuffer.len == 0) {
            sent = send(socketOf(r), data, read, 0);
            if (sent == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                if (!softError(error)) {
                    r.socketError = error;
                    fail(r, kSocketFailed);
                    return 0;
                }
                sent = 0;
            } else if (r.state == kPosting) {
                r.postBytesPosted += sent;
            }
            if (sent == read) how = 1;
        }
        if (how == 0) {
            if (!r.sendBuffer.add(data + sent, read - sent)) return 0;
            how = 2;
        }
        if (state.pos == state.size) return 1;
        if (how != 1) return 2;
    }
}

// 0x0056FC20: a file in memory sent 0x8000 bytes at a time while the socket takes them.
int32_t postMemory(Request& r, PostState& state) {
    const PostData& d = *state.data;
    if (d.len == 0) return 1;
    int sent = 0;
    do {
        const int32_t length = std::min(d.len - state.pos, 0x8000);
        sent = send(socketOf(r), d.string.data() + state.pos, length, 0);
        if (sent == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (!softError(error)) {
                r.socketError = error;
                fail(r, kSocketFailed);
                return 0;
            }
            sent = 0;
        } else if (r.state == kPosting) {
            r.postBytesPosted += sent;
        }
        state.pos += sent;
        if (d.len == state.pos) return 1;
    } while (sent != 0);
    return 2;
}

// 0x0056FCC0: an object's head first ("<name>=" / "&<name>=", or its part's boundary and Content-Disposition), then
// its data.
int32_t postStateDoPosting(Request& r, PostState& state, bool first) {
    const PostData& d = *state.data;
    if (state.pos == -1) {
        state.pos = 0;
        std::string head;
        if (!r.post->hasFiles) {
            head = std::format("{}{}=", first ? "" : "&", d.name);
        } else {
            const char* boundary = first ? kBoundary : kBoundaryAfter;
            if (d.type == 0) {
                head = std::format("{}Content-Disposition: form-data; name=\"{}\"\r\n\r\n", boundary, d.name);
            } else if (d.type == 1 || d.type == 2) {
                head = std::format("{}Content-Disposition: form-data; name=\"{}\"; filename=\"{}\"\r\n"
                                   "Content-Type: {}\r\n\r\n",
                                   boundary, d.name, d.reportFilename, d.contentType);
            }
        }
        const int32_t got = sendOrBuffer(r, head.c_str(), static_cast<int32_t>(head.size()));
        if (got == 0 || got == 2) return got;
    }
    if (d.type == 0) return postString(r, state);
    if (d.type != 1) return postMemory(r, state);
    return postFile(r, state);
}

// 0x0056FE70: the buffered bytes first, then each object in turn, a multipart post's last boundary: 1 done, 2 not
// yet, 0 a failure.
int32_t doPosting(Request& r) {
    const auto count = static_cast<int32_t>(r.postStates->size());
    if (r.sendBuffer.len != 0) {
        if (!sendBufferedData(r)) return 0;
        if (r.sendBuffer.pos < r.sendBuffer.len) return 2;
        r.sendBuffer.reset();
        if (r.postStateIndex == count) return 1;
    }
    while (r.postStateIndex < count) {
        const int32_t got =
            postStateDoPosting(r, (*r.postStates)[static_cast<size_t>(r.postStateIndex)], r.postStateIndex == 0);
        if (got == 0) return 0;
        if (got == 2) return 2;
        ++r.postStateIndex;
    }
    if (r.post->hasFiles && sendOrBuffer(r, kLastBoundary, 0x2B) == 0) return 0;
    return r.sendBuffer.len != 0 ? 2 : 1;
}

// 0x00570980: the post's data; its progress when bytes went; done, on to the answer.
void posting(Request& r) {
    const int32_t before = r.postBytesPosted;
    const int32_t got = doPosting(r);
    if (got == 0) {
        freePostStates(r);
        return;
    }
    if (before != r.postBytesPosted) postProgress(r);
    if (got == 1) {
        freePostStates(r);
        r.state = kWaiting;
        progress(r);
    }
}

// 0x005709E0: the answer's first bytes waited for.
void waiting(Request& r) {
    bool readable = false;
    if (selectSocket(socketOf(r), &readable, nullptr, nullptr) == -1) {
        fail(r, kSocketFailed);
        r.socketError = WSAGetLastError();
        return;
    }
    if (readable) {
        r.state = kReceivingStatus;
        progress(r);
    }
}

// 0x00570290: bytes received into a buffer of `size` (at most size - 1, a 0 after them; throttled: the throttle's
// bytes once in its time): 0 bytes come (`size` their count), 1 none yet, 2 the connection closed, 3 a failure.
int receiveData(Request& r, char* buffer, int32_t& size) {
    int32_t want = size - 1;
    if (r.throttle) {
        const DWORD now = GetTickCount();
        if (now < r.lastThrottleRecv + sdk().throttleTime) return 1;
        r.lastThrottleRecv = now;
        if (sdk().throttleBufferSize <= want) want = sdk().throttleBufferSize;
    }
    const int got = recv(socketOf(r), buffer, want, 0);
    if (got == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        if (!softError(error)) {
            r.socketError = error;
            fail(r, kSocketFailed);
            r.connectionClosed = true;
            return 3;
        }
        return 1;
    }
    if (got != 0) {
        buffer[got] = '\0';
        size = got;
        return 0;
    }
    r.connectionClosed = true;
    return 2;
}

// 0x00570A40: "HTTP/<major>.<minor> <code>" (a version over 0, a code 100 .. 599), the status text's place after
// the spaces; else kBadResponse.
bool parseStatus(Request& r) {
    const char* line = r.recvBuffer.data;
    int major = 0;
    int minor = 0;
    int code = 0;
    int end = 0;  // the stock's is not set when the line does not match
    const int matched = std::sscanf(line, "HTTP/%d.%d %d%n", &major, &minor, &code, &end);
    while (line[end] != '\0' && std::isspace(static_cast<unsigned char>(line[end])) != 0) ++end;
    if (matched == 3 && major > 0 && code > 99 && code < 600) {
        r.statusMajorVersion = major;
        r.statusCode = code;
        r.statusMinorVersion = minor;
        r.statusStringIndex = end;
        return true;
    }
    fail(r, kBadResponse);
    return false;
}

// The receive buffer's new bytes (0x00570B10's and 0x00570F60's start): false when the frame ends there.
bool receiveInto(Request& r, int got, const char* data, int32_t size) {
    if (got == 3) return false;
    if (got == 1) return r.recvBuffer.pos != r.recvBuffer.len;
    if (got == 0) return r.recvBuffer.add(data, size);
    return true;
}

// 0x00570B10: the status line (up to the first "\r\n", made a 0); the connection closed without it kBadResponse.
void receivingStatus(Request& r) {
    char data[0x400];
    int32_t size = sizeof data;
    const int got = receiveData(r, data, size);
    if (!receiveInto(r, got, data, size)) return;
    char* end = std::strstr(r.recvBuffer.data, "\r\n");
    if (end == nullptr) {
        if (got == 2) {
            fail(r, kBadResponse);
            r.socketError = WSAGetLastError();
        }
        return;
    }
    *end = '\0';
    if (parseStatus(r)) {
        r.recvBuffer.pos = static_cast<int32_t>(end + 2 - r.recvBuffer.data);
        r.state = kReceivingHeaders;
        progress(r);
    }
}

bool processFile(Request& r, char* data, int32_t length);

// 0x00570F60: the headers up to "\r\n\r\n" (else "\n\n"; Stock problem: then the body's first byte made a 0 and its
// first two skipped): a 1xx's again from the status; a 3xx's Location (more than 10 redirects kFileNotFound; a path
// made "http://<server>:<port><path>"); Content-Length (from 16 characters on: Stock problem: the one after the
// colon skipped; 19 digits from INT64_MAX on kFileTooBig); chunked; a HEAD or a post only done; else the body, the
// bytes come with the headers first.
void receivingHeaders(Request& r) {
    char data[0x1000];
    int32_t size = sizeof data;
    const int got = receiveData(r, data, size);
    if (!receiveInto(r, got, data, size)) return;
    char* headers = r.recvBuffer.data + r.recvBuffer.pos;
    char* end = std::strstr(headers, "\r\n\r\n");
    if (end == nullptr) end = std::strstr(headers, "\n\n");
    if (end == nullptr) {
        if (got == 2) {
            fail(r, kBadResponse);
            r.socketError = WSAGetLastError();
        }
        return;
    }
    end[2] = '\0';
    char* const buffer = r.recvBuffer.data;
    char* body = end + 4;
    const int32_t before = r.recvBuffer.len;
    r.recvBuffer.len = static_cast<int32_t>(end + 2 - buffer);
    const int32_t remaining = before - static_cast<int32_t>(body - buffer);
    const int32_t kind = r.statusCode / 100;
    if (kind == 1) {
        if (remaining == 0) {
            r.recvBuffer.len = 0;
            r.recvBuffer.pos = 0;
            buffer[0] = '\0';
        } else {
            std::memmove(buffer, body, static_cast<size_t>(remaining) + 1);
            r.recvBuffer.len = remaining;
            r.recvBuffer.pos = 0;
        }
        r.state = kReceivingStatus;
        progress(r);
        return;
    }
    if (kind == 3) {
        if (r.redirectCount > 10) {
            fail(r, kFileNotFound);
            return;
        }
        if (char* location = std::strstr(headers, "Location:"); location != nullptr) {
            char* from = location + 9;
            while (std::isspace(static_cast<unsigned char>(*from)) != 0) ++from;
            char* to = from;
            while (*to != '\0' && std::isspace(static_cast<unsigned char>(*to)) == 0) ++to;
            *to = '\0';
            r.redirectUrl = *from != '/' ? std::string(from)
                                         : std::format("http://{}:{}{}", r.serverAddress, r.serverPort, from);
            r.redirecting = true;
            return;
        }
    }
    const char* length = std::strstr(headers, "Content-Length:");
    if (length != nullptr) {
        constexpr std::string_view kMax = "9223372036854775807";
        const char* value = length + 16;
        const char* p = value;
        while (*p != '\0' && *p != '\n' && *p != '\r' && *p != ' ') ++p;
        const auto digits = static_cast<size_t>(p - value);
        if (digits > kMax.size() || (digits == kMax.size() && std::strncmp(value, kMax.data(), digits) >= 0)) {
            fail(r, kFileTooBig);
            return;
        }
        r.totalSize = _atoi64(value);
    }
    r.chunked = std::strstr(headers, "Transfer-Encoding: chunked") != nullptr;
    if (r.chunked) {
        r.chunkHeader[0] = '\0';
        r.chunkHeaderLen = 0;
        r.chunkBytesLeft = 0;
        r.chunkReadMode = 0;
    }
    if (r.type == Type::Head || r.type == Type::Post) {
        r.completed = 1;
        return;
    }
    r.state = kReceivingFile;
    if (length != nullptr && r.totalSize == 0) {
        r.completed = 1;
        return;
    }
    if (remaining > 0) processFile(r, body, remaining);
}

// 0x00570C10: the body's bytes counted (all of the length, or the connection closed: done), put in the get buffer,
// the save file (short: kFileWriteFailed) or given to the stream; the progress with them (the get's whole buffer).
bool gotFileData(Request& r, const char* data, int32_t length) {
    r.fileBytesReceived += length;
    if (r.fileBytesReceived == r.totalSize || r.connectionClosed) r.completed = 1;
    const char* buffer = nullptr;
    int32_t bufferLen = 0;
    if (r.type == Type::Get) {
        if (!r.getFileBuffer.add(data, length)) return false;
        buffer = r.getFileBuffer.data;
        bufferLen = r.getFileBuffer.len;
    } else if (r.type == Type::Save) {
        if (std::fwrite(data, 1, static_cast<size_t>(length), r.saveFile) != static_cast<size_t>(length)) {
            fail(r, kFileWriteFailed);
            return false;
        }
        buffer = data;
        bufferLen = length;
    } else if (r.type == Type::Stream) {
        buffer = data;
        bufferLen = length;
    }
    if (r.progressCallback != nullptr) {
        r.progressCallback(r.request, r.state, buffer, bufferLen, r.fileBytesReceived, r.totalSize, r.callbackParam);
    }
    return true;
}

// 0x00570D00: a chunk's size line kept (10 characters at most).
void addChunkHeader(Request& r, const char* data, int32_t length) {
    if (length == 0 || r.chunkHeaderLen >= 10) return;
    const int32_t n = std::min(length, 10 - r.chunkHeaderLen);
    std::memcpy(r.chunkHeader + r.chunkHeaderLen, data, static_cast<size_t>(n));
    r.chunkHeaderLen += n;
    r.chunkHeader[r.chunkHeaderLen] = '\0';
}

// 0x00570D60: the body's bytes; chunked, each chunk's hex size line (not hex: kBadResponse), its bytes, the line's
// end after them, done after the 0 chunk once more bytes come.
bool processFile(Request& r, char* data, int32_t length) {
    if (!r.chunked) return gotFileData(r, data, length);
    for (;;) {
        for (;;) {
            if (length < 1) return true;
            if (r.chunkReadMode == 0) break;
            if (r.chunkReadMode == 1) {
                const int32_t n = std::min(length, r.chunkBytesLeft);
                if (!gotFileData(r, data, n)) return false;
                length -= n;
                data += n;
                r.chunkBytesLeft -= n;
                if (r.chunkBytesLeft == 0) r.chunkReadMode = 2;
            } else if (r.chunkReadMode == 2) {
                char* newline = std::strchr(data, '\n');
                if (newline == nullptr) return true;
                length = static_cast<int32_t>(data + length - (newline + 1));
                r.chunkHeader[0] = '\0';
                r.chunkHeaderLen = 0;
                r.chunkBytesLeft = 0;
                r.chunkReadMode = 0;
                data = newline + 1;
            } else {
                if (r.chunkReadMode != 3) return false;
                r.completed = 1;
                return true;
            }
        }
        char* newline = std::strchr(data, '\n');
        if (newline == nullptr) {
            addChunkHeader(r, data, length);
            return true;
        }
        addChunkHeader(r, data, static_cast<int32_t>(newline - data));
        length = static_cast<int32_t>(data + length - (newline + 1));
        int size = 0;
        if (std::sscanf(r.chunkHeader, "%x", &size) != 1) size = -1;
        r.chunkBytesLeft = size;
        if (size == -1) break;
        r.chunkReadMode = size == 0 ? 3 : 1;
        data = newline + 1;
    }
    fail(r, kBadResponse);
    return false;
}

// 0x005713A0: the body read 0x2000 bytes at a time for up to the request's time (500 ms); the connection closed:
// done, short of a known length kFileIncomplete.
void receivingFile(Request& r) {
    const DWORD start = GetTickCount();
    uint32_t elapsed = 0;
    if (r.completed != 0) return;
    while (elapsed < r.maxRecvTime) {
        char data[0x2000];
        int32_t size = sizeof data;
        const int got = receiveData(r, data, size);
        if (got == 3 || got == 1) return;
        if (got == 2) {
            r.completed = 1;
            if (r.totalSize < 0 || r.totalSize == 0 || r.totalSize <= r.fileBytesReceived) return;
            r.result = kFileIncomplete;
            return;
        }
        if (!processFile(r, data, size)) return;
        elapsed = GetTickCount() - start;
        if (r.completed != 0) return;
    }
}

// 0x0056F7C0: a redirect: the new URL from the host's address on, the connection and buffers anew, counted.
void redirect(Request& r) {
    r.state = kHostLookup;
    r.url = std::move(r.redirectUrl);
    r.redirectUrl.clear();
    r.redirecting = false;
    r.serverAddress.clear();
    r.serverIp = 0;
    r.serverPort = 0;
    r.requestPath.clear();
    shutdown(socketOf(r), SD_BOTH);
    closesocket(socketOf(r));
    r.socket = ~uintptr_t{0};
    r.sendBuffer.reset();
    r.recvBuffer.reset();
    ++r.redirectCount;
    r.statusMajorVersion = 0;
    r.statusMinorVersion = 0;
    r.statusCode = 0;
    r.statusStringIndex = 0;
    r.connectionClosed = false;
}

// 0x00568850: a 4xx or 5xx status's result (401 kUnauthorized, 403 kForbidden, 404 and 410 kFileNotFound, other 4xx
// kRequestRejected, 5xx kServerError).
void resultFromStatus(Request& r) {
    switch (r.statusCode / 100) {
    case 4:
        switch (r.statusCode) {
        case 401: r.result = kUnauthorized; return;
        case 403: r.result = kForbidden; return;
        case 404:
        case 410: r.result = kFileNotFound; return;
        default: r.result = kRequestRejected; return;
        }
    case 5: r.result = kServerError; return;
    default: return;
    }
}

// 0x005714A0: the completion with the get's buffer and the bytes received; false keeps the buffer.
void complete(Request& r) {
    if (r.completedCallback == nullptr) return;
    char* buffer = nullptr;
    int64_t length = 0;
    if (r.type == Type::Get) {
        buffer = r.getFileBuffer.data;
        length = r.fileBytesReceived;
    }
    const bool freeBuffer = r.completedCallback(r.request, r.result, buffer, length, r.callbackParam);
    if (buffer != nullptr && !freeBuffer) r.getFileBuffer.dontFree = true;
}

// A buffer's bytes grown by its increment (none without one).
char* grownBy(char* data, int32_t size, int32_t increment) {
    if (increment < 1) return nullptr;
    return static_cast<char*>(std::realloc(data, static_cast<size_t>(size + increment)));
}

}  // namespace

bool Buffer::init(Request* owner, int32_t increment) {
    connection = owner;
    data = nullptr;
    size = 0;
    len = 0;
    pos = 0;
    sizeIncrement = increment;
    fixed = false;
    dontFree = false;
    data = static_cast<char*>(std::realloc(nullptr, 0x800));
    if (data == nullptr) return false;
    size = 0x800;
    data[0] = '\0';
    return true;
}

bool Buffer::add(const char* bytes, int32_t length) {
    if (bytes == nullptr || length < 0) return false;
    if (length == 0) length = static_cast<int32_t>(std::strlen(bytes));
    const int32_t newLen = len + length;
    while (newLen >= size) {
        if (fixed) {
            fail(*connection, kBufferOverflow);
            return false;
        }
        char* grown = grownBy(data, size, sizeIncrement);
        if (grown == nullptr) {
            fail(*connection, kOutOfMemory);
            return false;
        }
        data = grown;
        size += sizeIncrement;
    }
    std::memcpy(data + len, bytes, static_cast<size_t>(length));
    len = newLen;
    data[newLen] = '\0';
    return true;
}

bool Buffer::addChar(char c) {
    if (size <= len + 1) {
        if (fixed) {
            fail(*connection, kBufferOverflow);
            return false;
        }
        char* grown = grownBy(data, size, sizeIncrement);  // 0x0056FF50: once
        if (grown == nullptr) {
            fail(*connection, kOutOfMemory);
            return false;
        }
        data = grown;
        size += sizeIncrement;
    }
    data[len] = c;
    ++len;
    data[len] = '\0';
    return true;
}

bool Buffer::addHeader(const char* name, const char* value) {
    return add(name) && add(": ") && add(value) && add("\r\n");
}

bool Buffer::addInt(int32_t value) {
    return add(std::to_string(value).c_str());
}

void Buffer::reset() {
    len = 0;
    pos = 0;
    data[0] = '\0';
}

void Buffer::free() {
    if (data == nullptr) return;
    if (!dontFree) std::free(data);
    *this = Buffer{};
}

void startup() {
    Sdk& s = sdk();
    std::recursive_mutex* entered = s.lock.get();
    if (entered != nullptr) entered->lock();
    ++s.references;
    if (s.references == 1) {
        s.lock = std::make_unique<std::recursive_mutex>();
        s.throttleBufferSize = 125;
        s.throttleTime = 250;
        return;
    }
    if (entered != nullptr) entered->unlock();
}

void cleanup() {
    Sdk& s = sdk();
    if (s.lock != nullptr) s.lock->lock();
    --s.references;
    if (s.references == 0) {
        freeAll();
        s.proxy.clear();
        if (s.lock != nullptr) {
            s.lock->unlock();
            s.lock.reset();
        }
        return;
    }
    if (s.lock != nullptr) s.lock->unlock();
}

int32_t references() {
    return sdk().references;
}

int32_t get(const char* url, bool blocking, CompletedCallback completedCallback, void* param) {
    if (url == nullptr || *url == '\0') return -1;
    if (sdk().references == 0) startup();
    Request* r = newRequest();
    if (r == nullptr) return -1;
    r->type = Type::Get;
    r->url = url;
    r->post = nullptr;
    r->blocking = blocking;
    r->progressCallback = nullptr;
    r->completedCallback = completedCallback;
    r->callbackParam = param;
    r->throttle = false;
    r->userBufferSupplied = false;
    if (!r->getFileBuffer.init(r, 0x800)) {
        freeRequest(*r);
        return -1;
    }
    if (blocking) {
        while (process(*r) == 0) Sleep(10);
        return 0;
    }
    return r->request;
}

void think() {
    eachRequest([](Request& r) { process(r); });
}

int32_t process(Request& r) {
    if (r.processing) return 0;
    r.processing = true;
    if (r.state == kHostLookup) hostLookup(r);
    if (r.state == kConnecting) connecting(r);
    if (r.state == kSendingRequest) sendingRequest(r);
    if (r.state == kPosting) posting(r);
    if (r.state == kWaiting) waiting(r);
    if (r.state == kReceivingStatus) receivingStatus(r);
    if (r.state == kReceivingHeaders) receivingHeaders(r);
    if (r.state == kReceivingFile) receivingFile(r);
    if (r.redirecting) redirect(r);
    const int32_t completed = r.completed;
    if (completed != 0) {
        resultFromStatus(r);
        if (r.saveFile != nullptr) {
            std::fclose(r.saveFile);
            r.saveFile = nullptr;
        }
        complete(r);
        freeRequest(r);
        return completed;
    }
    r.processing = false;
    return 0;
}

std::string& proxyServer() {
    return sdk().proxy;
}

uint16_t& proxyPort() {
    return sdk().proxyPort;
}

Request* request(int32_t number) {
    Sdk& s = sdk();
    if (number < 0 || number >= static_cast<int32_t>(s.requests.size())) return nullptr;
    Request* r = s.requests[static_cast<size_t>(number)].get();
    return r->inUse ? r : nullptr;
}

}  // namespace oj::net::ghttp
