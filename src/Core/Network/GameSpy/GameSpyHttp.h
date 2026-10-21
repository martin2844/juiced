#pragma once
// GameSpy's HTTP SDK linked in the stock game (ghttp, docs/spec/28-network.md, "The statistics"): of its requests only
// ghttpGet (0x00568AD0), made by the leaderboards ([0x0075F280], 0x0055D290), their frame running ghttpThink
// (0x0056F760 with 0x005688F0) and ghttpCleanup (0x00568A40). A request's states (0x005688F0): the host's address
// (0x005704E0), the connection (0x00570620), the request sent (0x005707B0), a post's data (0x00570980), the answer
// waited for (0x005709E0), its status line (0x00570B10), its headers (0x00570F60), its body (0x005713A0, chunked
// 0x00570D60), the completion (0x005714A0). The code of the requests the game never makes (saved to a file, streamed,
// HEAD, the posts' data 0x0056F8F0 .. 0x005703B0) is kept as it is.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net::ghttp {

// GHTTPResult.
enum Result : int32_t {
    kSuccess = 0,
    kOutOfMemory = 1,
    kBufferOverflow = 2,
    kParseUrlFailed = 3,
    kHostLookupFailed = 4,
    kSocketFailed = 5,
    kConnectFailed = 6,
    kBadResponse = 7,
    kRequestRejected = 8,
    kUnauthorized = 9,
    kForbidden = 10,
    kFileNotFound = 11,
    kServerError = 12,
    kFileWriteFailed = 13,
    kFileReadFailed = 14,
    kFileIncomplete = 15,
    kFileTooBig = 16,
};

// GHTTPState: the request's +0x10.
enum State : int32_t {
    kHostLookup = 0,
    kConnecting = 1,
    kSendingRequest = 2,
    kPosting = 3,
    kWaiting = 4,
    kReceivingStatus = 5,
    kReceivingHeaders = 6,
    kReceivingFile = 7,
};

// The request's type (+0x0C): ghttpGet's 0, the others the SDK's save, stream, HEAD and post-only requests.
enum class Type : int32_t { Get = 0, Save = 1, Stream = 2, Head = 3, Post = 4 };

// The callbacks (the SDK's): the progress (+0x38; none from ghttpGet), the completion (+0x3C: false keeps the get
// buffer for the caller, who frees it with std::free), a post's progress (the post's +0x04).
using ProgressCallback = void (*)(int32_t request, int32_t state, const char* buffer, int64_t bufferLen,
                                  int64_t bytesReceived, int64_t totalSize, void* param);
using CompletedCallback = bool (*)(int32_t request, int32_t result, char* buffer, int64_t bufferLen, void* param);
using PostCallback = void (*)(int32_t request, int32_t bytesPosted, int32_t totalBytes, int32_t objectsPosted,
                              int32_t totalObjects, void* param);

struct Request;

// GHIBuffer (0x20 bytes, malloc'd as the SDK's): the send buffer (+0x4C), the receive buffer (+0x6C), the get
// buffer (+0x8C). Its bytes always end with a 0 after len.
struct Buffer {
    Request* connection = nullptr;  // +0x00
    char* data = nullptr;           // +0x04
    int32_t size = 0;               // +0x08
    int32_t len = 0;                // +0x0C
    int32_t pos = 0;                // +0x10
    int32_t sizeIncrement = 0;      // +0x14
    bool fixed = false;             // +0x18: a user's buffer, no growing (overflow)
    bool dontFree = false;          // +0x1C: the caller keeps the bytes

    // 0x0056F480's / 0x00568AD0's: 0x800 bytes, "" in them.
    bool init(Request* owner, int32_t increment);
    // 0x0056FFD0: the bytes added (length 0: up to their 0), grown by the increment; a fixed buffer's overflow
    // fails the request (kBufferOverflow), no growth kOutOfMemory.
    bool add(const char* bytes, int32_t length = 0);
    bool addChar(char c);                              // 0x00570100
    bool addHeader(const char* name, const char* value);  // 0x005700A0: "<name>: <value>\r\n"
    bool addInt(int32_t value);                        // 0x00570180: "%d"
    void reset();                                      // len, pos 0, "" in it
    void free();                                       // 0x0056FF90
};

// GHIPostData: a post's object; nothing in the game makes one (ghttpGet's post +0x10C is 0).
struct PostData {
    int32_t type = 0;            // +0x00: 0 a string, 1 a file on disk, 2 a file in memory
    std::string name;            // +0x04
    std::string string;          // +0x08: the string; the file's name; the memory file's bytes
    int32_t len = 0;             // +0x0C: the string's / the memory file's length
    bool invalidChars = false;   // +0x10: the string URL-encoded
    std::string reportFilename;  // the disk file's +0x0C, the memory file's +0x10
    std::string contentType;     // the disk file's +0x10, the memory file's +0x14
};

// GHIPost (0x14 bytes).
struct Post {
    std::vector<PostData> data;      // +0x00
    PostCallback callback = nullptr;  // +0x04
    void* param = nullptr;           // +0x08
    bool hasFiles = false;           // +0x0C: multipart
    bool autoFree = false;           // +0x10
};

// GHIPostState: each post object's progress (the request's +0x110 list).
struct PostState {
    PostData* data = nullptr;  // +0x00
    int32_t pos = -1;          // +0x04: -1 its head not sent
    FILE* file = nullptr;      // +0x08
    int32_t size = 0;          // +0x0C
};

// GHIConnection (0x138 bytes, 0x0056F480).
struct Request {
    bool inUse = false;                    // +0x00
    int32_t request = 0;                   // +0x04: its slot
    int32_t uniqueId = 0;                  // +0x08: [0x0075B90C], counted up
    Type type = Type::Get;                 // +0x0C
    int32_t state = kHostLookup;           // +0x10
    std::string url;                       // +0x14
    std::string serverAddress;             // +0x18
    uint32_t serverIp = 0;                 // +0x1C
    uint16_t serverPort = 0;               // +0x20
    std::string requestPath;               // +0x24
    std::string sendHeaders;               // +0x28
    FILE* saveFile = nullptr;              // +0x2C
    bool blocking = false;                 // +0x30
    int32_t result = kSuccess;             // +0x34
    ProgressCallback progressCallback = nullptr;    // +0x38
    CompletedCallback completedCallback = nullptr;  // +0x3C
    void* callbackParam = nullptr;         // +0x40
    uintptr_t socket = ~uintptr_t{0};      // +0x44
    int32_t socketError = 0;               // +0x48
    Buffer sendBuffer;                     // +0x4C
    Buffer recvBuffer;                     // +0x6C
    Buffer getFileBuffer;                  // +0x8C
    bool userBufferSupplied = false;       // +0xAC
    int32_t statusMajorVersion = 0;        // +0xB0
    int32_t statusMinorVersion = 0;        // +0xB4
    int32_t statusCode = 0;                // +0xB8
    int32_t statusStringIndex = 0;         // +0xBC
    int32_t completed = 0;                 // +0xC0
    int64_t fileBytesReceived = 0;         // +0xC8
    int64_t totalSize = -1;                // +0xD0
    std::string redirectUrl;               // +0xD8
    bool redirecting = false;              // +0xD8 set
    int32_t redirectCount = 0;             // +0xDC
    bool chunked = false;                  // +0xE0
    char chunkHeader[11] = {};             // +0xE4
    int32_t chunkHeaderLen = 0;            // +0xF0
    int32_t chunkBytesLeft = 0;            // +0xF4
    int32_t chunkReadMode = 0;             // +0xF8: 0 the size line, 1 the data, 2 the line's end after it, 3 done
    bool processing = false;               // +0xFC
    bool connectionClosed = false;         // +0x100
    bool throttle = false;                 // +0x104
    uint32_t lastThrottleRecv = 0;         // +0x108
    Post* post = nullptr;                  // +0x10C
    std::unique_ptr<std::vector<PostState>> postStates;  // +0x110
    int32_t postStateIndex = 0;            // +0x114
    int32_t postBytesPosted = 0;           // +0x118
    int32_t postTotalBytes = 0;            // +0x11C
    PostCallback postCallback = nullptr;   // +0x120
    uint32_t maxRecvTime = 500;            // +0x128
    std::string proxyOverrideServer;       // +0x12C
    uint16_t proxyOverridePort = 80;       // +0x130
};

// 0x005689D0: the SDK's start (counted, [0x0075B568]); the first makes the lock and the throttle's 125 bytes a
// 250 ms.
void startup();
// 0x00568A40: the last cleanup frees every request (0x0056F860) and the proxy.
void cleanup();
// [0x0075B568].
int32_t references();

// 0x00568AD0: a get of the URL into a buffer: -1 for no URL (empty) or no request; blocking, run to its end (10 ms
// sleeps) and 0; else the request's number.
int32_t get(const char* url, bool blocking, CompletedCallback completedCallback, void* param);
// ghttpThink: 0x0056F760 running 0x005688F0 on each request in use.
void think();
// 0x005688F0: a request's frame: its states run in turn; completed, its result by the status (0x00568850), the
// completion called and the request freed (the completion's value, else 0).
int32_t process(Request& request);

// The proxy ([0x007A4338], [0x007A4334]): ghttpSetProxy is not linked, never set.
std::string& proxyServer();
uint16_t& proxyPort();

// For the tests: the request of a number (none when not in use).
Request* request(int32_t number);

}  // namespace oj::net::ghttp
