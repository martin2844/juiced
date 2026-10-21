// GameSpy Presence's peers (0x0057AEF0 .. 0x0057C020: TCP connections straight between two players, authorized by
// the server's signature, carrying buddy messages) and the files sent over them (the transfers, 0x00576E70 ..
// 0x00578FE0): the SDK's code the linker kept, reached by none of the game's calls. The game logs in behind the
// firewall (no listening socket), its players' buddies have no port (no peer of ours) and it sets no transfer
// callback (docs/spec/28-network.md, "GameSpy Presence").

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyPresenceDetail.h"

#include "Core/Base/Random.h"

#include <winsock2.h>
#include <windows.h>

#include <share.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>

namespace oj::net::gp {

using namespace detail;

namespace {

constexpr int32_t kPeerNew = 0x64;
constexpr int32_t kPeerWaitingSig = 0x65;
constexpr int32_t kPeerToConnect = 0x66;
constexpr int32_t kPeerConnecting = 0x67;
constexpr int32_t kPeerAuthorizing = 0x68;
constexpr int32_t kPeerConnected = 0x69;
constexpr int32_t kPeerClosed = 0x6A;

// The peers' messages of the transfers (GPI_BM_FILE_*).
constexpr int32_t kSendRequest = 200;
constexpr int32_t kSendReply = 201;
constexpr int32_t kBegin = 202;
constexpr int32_t kEnd = 203;
constexpr int32_t kData = 204;
constexpr int32_t kSkip = 205;
constexpr int32_t kThrottle = 206;
constexpr int32_t kCancel = 207;
constexpr int32_t kKeepAlive = 208;

// A file's bytes read for a data message ([0x0075B928], 8 KB).
char g_fileBuffer[0x2000];

int64_t now() {
    return static_cast<int64_t>(std::time(nullptr));
}

}  // namespace

// ---- The peers.

// 0x0057BAB0: a peer at the list's head: new (0x64), its timeout 300 s on.
Connection::Peer& Connection::addPeer(int32_t profileId, bool initiated) {
    auto peer = std::make_unique<Peer>();
    peer->state = kPeerNew;
    peer->initiated = initiated;
    peer->profile = profileId;
    peer->timeout = now() + 300;
    m_peers.insert(m_peers.begin(), std::move(peer));
    return *m_peers.front();
}

// The output's unsent bytes (+0x30 - +0x34): the peer's own and those its socket holds yet.
size_t Connection::outputLength(const Peer& peer) {
    return peer.output.size() + peer.socket.pending();
}

// 0x00573430 (0x005733C0 for one byte): nothing unsent before them (output, messages), the bytes go to the socket at
// once, else after the output. A socket's error ("PT": not a peer's own sending) is told: "There was an error sending
// on a socket." (5, not fatal).
Result Connection::peerSend(Peer& peer, std::string_view bytes) {
    if (bytes.empty()) return Result::NoError;
    if (outputLength(peer) != 0 || !peer.messages.empty()) {
        peer.output.append(bytes);
        return Result::NoError;
    }
    peer.socket.send(bytes.data(), bytes.size());
    if (peer.socket.state() == TcpConnection::State::Failed) {
        setError("There was an error sending on a socket.", kNetwork);
        callError(Result::NetworkError, false);
        return Result::NetworkError;
    }
    return Result::NoError;
}

// 0x0057B370: nothing left of the output, the messages waiting sent in turn, each whole before the next goes (a
// failure closing the peer); one the socket has not taken all of stays first until it has.
Result Connection::sendPeerMessages(Peer& peer) {
    if (outputLength(peer) != 0) return Result::NoError;
    while (!peer.messages.empty()) {
        PeerMessage& m = peer.messages.front();
        if (!m.handed) {
            peer.socket.send(m.bytes);
            m.handed = true;
            if (peer.socket.state() == TcpConnection::State::Failed) {
                peer.state = kPeerClosed;
                return Result::NoError;
            }
        }
        if (peer.socket.pending() != 0) return Result::NoError;
        peer.messages.erase(peer.messages.begin());
    }
    return Result::NoError;
}

// 0x005735D0: the bytes come into the peer's input; an error of the socket: "There was an error reading from a
// socket." (no code); read 0: closed.
Result Connection::receivePeer(Peer& peer, size_t& read, bool& closed) {
    peer.socket.poll();
    std::vector<uint8_t>& in = peer.socket.received();
    read = in.size();
    peer.input.append(reinterpret_cast<const char*>(in.data()), in.size());
    in.clear();
    if (peer.socket.readError()) {
        setError("There was an error reading from a socket.");
        return Result::NetworkError;
    }
    closed = peer.socket.lost();
    return Result::NoError;
}

// 0x00573810: a message whole in the input: the header to the line's end ending "\msg\", its \m\ (the type) and
// \len\, then that many bytes and a 0. A header without them, or no 0 after the text: the network error (no text).
Result Connection::readPeerMessage(Peer& peer, PeerRead& out) {
    out = PeerRead{};
    if (peer.input.size() < 5) return Result::NoError;
    const size_t line = cString(peer.input).find('\n');
    if (line == std::string_view::npos) return Result::NoError;
    if (line < 5 || peer.input.compare(line - 5, 5, "\\msg\\") != 0) return Result::NetworkError;
    out.header = peer.input.substr(0, line);
    std::string value;
    if (!valueForKey(out.header, "\\m\\", value, 0x10)) return Result::NetworkError;
    out.type = atolOf(value);
    if (!valueForKey(out.header, "\\len\\", value, 0x10)) return Result::NetworkError;
    const int64_t length = atolOf(value);
    const int64_t end = static_cast<int64_t>(line) + length + 1;
    if (static_cast<int64_t>(peer.input.size()) <= end) return Result::NoError;
    if (end <= static_cast<int64_t>(line) || peer.input[static_cast<size_t>(end)] != '\0') return Result::NetworkError;
    out.text = peer.input.substr(line + 1, static_cast<size_t>(length));
    out.used = static_cast<size_t>(end) + 1;
    out.found = true;
    return Result::NoError;
}

// 0x0057B780: out of the list; its messages waiting under 100 (a buddy's own) sent through the server instead
// (0x00575910); then 0x0057B720: every transfer's connection lost (0x00578C60; Stock problem: not only this peer's),
// the socket shut and closed.
void Connection::removePeer(Peer* peer) {
    const auto it = std::find_if(m_peers.begin(), m_peers.end(), [peer](const auto& p) { return p.get() == peer; });
    if (it == m_peers.end()) return;
    std::unique_ptr<Peer> gone = std::move(*it);
    m_peers.erase(it);
    for (const PeerMessage& m : gone->messages) {
        if (m.type < 100) sendBuddyMessage(gone->profile, m.type, std::string(cString(m.bytes.substr(m.start))));
    }
    transfersLost();
    gone->socket.close(true);
}

// 0x0057BB50: the profile's info asked of the server for its signature (an operation of no callback, 0x0057ABC0);
// the peer waits for it (0x65).
Result Connection::askPeerSig(Peer& peer) {
    const Operation& op = addOperation(2, false);
    m_output += std::format("\\getprofile\\\\sesskey\\{}\\profileid\\{}\\id\\{}\\final\\", m_sessKey, peer.profile,
                            op.id);
    peer.state = kPeerWaitingSig;
    return Result::NoError;
}

// 0x0057BBA0: to the buddy's address (its status's |ip| and |p|): a socket (non-blocking, its buffers 0x0057B830)
// and its connect begun: connecting (0x67). The profile not known: "Error connecting to a peer." (Stock problem: a
// profile without its buddy part is read through a null pointer; here the same error). No socket: the network error
// told (not fatal); a connect refused at once: the network error, fatal.
Result Connection::connectPeer(Peer& peer) {
    const Profile* p = profile(peer.profile);
    if (p == nullptr || !p->buddy) {
        setError("Error connecting to a peer.");
        return Result::NetworkError;
    }
    if (!peer.socket.open()) {
        setError("There was an error creating a socket.", kNetwork);
        callError(Result::NetworkError, false);
        return Result::NetworkError;
    }
    peer.socket.tuneBuffers({0x4000, 0x8000, 0x10000, 0x20000, 0x40000}, {0x4000, 0x8000, 0x10000});
    if (!peer.socket.connectTo(p->buddy->ip, ntohs(p->buddy->port))) {
        setError("There was an error connecting a socket.", kNetwork);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    }
    peer.state = kPeerConnecting;
    return Result::NoError;
}

// 0x0057BDE0: "\m\<type>\len\<n>\msg\", a line's end, the text and a 0, queued for the peer (sent once it is
// connected and all before is gone); its timeout 300 s on.
Result Connection::queuePeerMessage(Peer& peer, int32_t type, std::string_view text) {
    PeerMessage m;
    m.type = type;
    m.bytes = std::format("\\m\\{}\\len\\{}\\msg\\\n", type, text.size());
    m.start = m.bytes.size();
    m.bytes.append(text);
    m.bytes.push_back('\0');
    peer.messages.push_back(std::move(m));
    peer.timeout = now() + 300;
    return Result::NoError;
}

// gpiSendBuddyMessage (0x00575A00): to a connected peer of the profile, queued on it; else, the profile a buddy with a
// port, queued on a new peer of ours (its signature asked of the server when not known, else connected at once; a
// peer of the profile not yet connected is not looked for); else through the server (0x00575910).
Result Connection::routeBuddyMessage(int32_t profileId, int32_t type, std::string_view text) {
    for (const auto& peer : m_peers) {
        if (peer->profile == profileId && peer->state == kPeerConnected) return queuePeerMessage(*peer, type, text);
    }
    const Profile* p = profile(profileId);
    if (p == nullptr || !p->buddy || p->buddy->port == 0) {
        sendBuddyMessage(profileId, type, std::string(text));
        return Result::NoError;
    }
    Peer& peer = addPeer(profileId, true);
    const Result r = p->peerSig ? connectPeer(peer) : askPeerSig(peer);
    if (r != Result::NoError) return r;
    return queuePeerMessage(peer, type, text);
}

// 0x0057AEF0: a peer of ours. To connect: connected (0x0057BBA0). Connecting: its progress (0x00579B60); failed, or
// its profile gone: "Error connecting to a peer."; made: \auth\ with our profile, nick and the server's signature for
// the peer, that signature then let go unless another peer of the profile not yet connected still needs it (and the
// profile with it when nothing else holds it), authorizing. Authorizing: the answer to its "\final\": \anack\ counted
// (the second "Error getting buddy authorization.", the first has the signature asked again; Stock problem: the peer
// is then taken as connected all the same), \aack\ connected, anything else "Error parsing buddy message."; the input
// dropped (bytes after the answer too). Then the output sent, a failure closing it.
Result Connection::processInitiating(Peer& peer) {
    if (peer.state == kPeerToConnect) {
        const Result r = connectPeer(peer);
        if (r != Result::NoError) return r;
    } else if (peer.state == kPeerConnecting) {
        peer.socket.poll();
        if (peer.socket.state() == TcpConnection::State::Failed) {
            setError("Error connecting to a peer.");
            return Result::NetworkError;
        }
        if (peer.socket.connected()) {
            Profile* p = profile(peer.profile);
            if (p == nullptr) {
                setError("Error connecting to a peer.");
                return Result::NetworkError;
            }
            peer.output += std::format("\\auth\\\\pid\\{}\\nick\\{}\\sig\\{}\\final\\", m_profileId, m_nick,
                                       p->peerSig.value_or(""));
            bool drop = true;
            for (const auto& other : m_peers) {
                if (other->profile == peer.profile && other.get() != &peer && other->state <= kPeerConnecting) {
                    drop = false;
                }
            }
            if (drop) {
                p->peerSig.reset();
                if (!p->buddy && !p->cache && !p->authSig) removeProfile(peer.profile);
            }
            peer.state = kPeerAuthorizing;
        }
    } else if (peer.state == kPeerAuthorizing) {
        size_t read = 0;
        bool closed = false;
        const Result r = receivePeer(peer, read, closed);
        if (r != Result::NoError) return r;
        const size_t final = cString(peer.input).find("\\final\\");
        if (final != std::string_view::npos) {
            const std::string_view answer = std::string_view(peer.input).substr(0, final);
            if (answer.starts_with("\\anack\\")) {
                if (++peer.nacks > 1) {
                    setError("Error getting buddy authorization.");
                    return Result::NetworkError;
                }
                const Result asked = askPeerSig(peer);
                if (asked != Result::NoError) return asked;
            } else if (!answer.starts_with("\\aack\\")) {
                setError("Error parsing buddy message.");
                return Result::NetworkError;
            }
            peer.state = kPeerConnected;
            peer.input.clear();
        }
    }
    if (outputLength(peer) > 0) {
        if (!peer.output.empty()) {
            peer.socket.send(peer.output);
            peer.output.clear();
        }
        if (peer.socket.state() == TcpConnection::State::Failed) peer.state = kPeerClosed;
    }
    return Result::NoError;
}

// 0x0057B160: a peer come to our listening socket: its \auth\ to "\final\" with \pid\, \nick\ and \sig\ (else
// closed); the signature must be MD5("<our password><our profile><its profile>") in hex: then \aack\, its profile
// taken, connected, the input dropped; else \anack\ (Stock problem: only queued, it goes unsent with the peer closed
// at once). Closed by the other side: closed.
Result Connection::processIncoming(Peer& peer) {
    size_t read = 0;
    bool closed = false;
    const Result r = receivePeer(peer, read, closed);
    if (r != Result::NoError) return r;
    if (closed) {
        peer.state = kPeerClosed;
        return Result::NoError;
    }
    const size_t final = cString(peer.input).find("\\final\\");
    if (final == std::string_view::npos) return Result::NoError;
    const std::string auth = peer.input.substr(0, final);
    std::string value;
    std::string nick;
    std::string sig;
    if (auth.compare(0, 6, "\\auth\\") != 0 || !valueForKey(auth, "\\pid\\", value, 0x10) ||
        !valueForKey(auth, "\\nick\\", nick, 0x1F) || !valueForKey(auth, "\\sig\\", sig, 0x21)) {
        peer.state = kPeerClosed;
        return Result::NoError;
    }
    const int32_t pid = atolOf(value);
    if (sig != md5Hex(std::format("{}{}{}", m_password, m_profileId, pid))) {
        peer.output += "\\anack\\\\final\\";
        peer.state = kPeerClosed;
        return Result::NoError;
    }
    peer.output += "\\aack\\\\final\\";
    peer.profile = pid;
    peer.state = kPeerConnected;
    peer.input.clear();
    return Result::NoError;
}

// 0x0057B410: a connected peer. The output sent (a failure closing it); none left, the messages waiting; the bytes
// come (its timeout 300 s on when there are any; an error closing it); each whole message then: 1 a buddy's message
// (the callback +0x1D8, now as its date), 102 a ping answered by 103 "1" (0x00575A00), 103 a pong to the transfers
// waiting for this profile (0x00578D00), 200 .. 208 a transfer's (0x00578E60); a message that does not read: the
// network error. Closed by the other side: closed once its messages are read.
Result Connection::processConnected(Peer& peer) {
    if (outputLength(peer) != 0) {
        if (!peer.output.empty()) {
            peer.socket.send(peer.output);
            peer.output.clear();
        }
        if (peer.socket.state() == TcpConnection::State::Failed) {
            peer.state = kPeerClosed;
            return Result::NoError;
        }
    }
    if (outputLength(peer) == 0) {
        const Result r = sendPeerMessages(peer);
        if (r != Result::NoError) return r;
        if (peer.state == kPeerClosed) return Result::NoError;
    }
    size_t read = 0;
    bool closed = false;
    if (receivePeer(peer, read, closed) != Result::NoError) {
        peer.state = kPeerClosed;
        return Result::NoError;
    }
    if (read > 0) peer.timeout = now() + 300;
    for (;;) {
        PeerRead m;
        const Result r = readPeerMessage(peer, m);
        if (r != Result::NoError) return r;
        if (!m.found) {
            if (closed) peer.state = kPeerClosed;
            return Result::NoError;
        }
        if (m.type == 1) {
            if (onBuddyMessage) {
                const BuddyMessageArg arg{peer.profile, static_cast<uint32_t>(std::time(nullptr)), fromUtf8(m.text)};
                addCallback(2, 0, [this, arg] {
                    if (onBuddyMessage) onBuddyMessage(arg);
                });
            }
        } else if (m.type == 102) {
            routeBuddyMessage(peer.profile, 103, "1");
        } else if (m.type == 103) {
            transfersPong(peer.profile, &peer);
        } else if (m.type >= kSendRequest && m.type <= kKeepAlive) {
            processTransferMessage(peer, m.type, m.header, m.text);
        }
        peer.input.erase(0, m.used);
    }
}

// 0x0057B980, connected or logged in: a connection come to the listening socket made a peer (its profile -1, not
// ours, authorizing; non-blocking, its buffers); then each peer of the list as it was: not connected, its steps (ours
// 0x0057AEF0, else 0x0057B160) and, connected by them without an error, the connected one's (0x0057B410), which a
// connected one has alone; closed, failed or past its timeout, it goes (0x0057B780). Its errors end nothing else.
Result Connection::processPeers() {
    if (m_peerListener.opened()) {
        auto incoming = std::make_unique<Peer>();
        if (m_peerListener.accept(incoming->socket)) {
            incoming->state = kPeerAuthorizing;
            incoming->profile = -1;
            incoming->timeout = now() + 300;
            incoming->socket.tuneBuffers({0x4000, 0x8000, 0x10000, 0x20000, 0x40000}, {0x4000, 0x8000, 0x10000});
            m_peers.insert(m_peers.begin(), std::move(incoming));
        }
    }
    std::vector<Peer*> round;
    round.reserve(m_peers.size());
    for (const auto& peer : m_peers) round.push_back(peer.get());
    for (Peer* peer : round) {
        Result r = Result::NoError;
        if (peer->state != kPeerConnected) {
            r = peer->initiated ? processInitiating(*peer) : processIncoming(*peer);
            if (r == Result::NoError && peer->state == kPeerConnected) r = processConnected(*peer);
        } else {
            r = processConnected(*peer);
        }
        if (peer->state == kPeerClosed || r != Result::NoError || now() > peer->timeout) removePeer(peer);
    }
    return Result::NoError;
}

// 0x0057BEF0: a transfer's message begun: "\m\<type>\xfer\<profile> <count> <time>".
Result Connection::startTransferMessage(Peer& peer, int32_t type, const TransferKey& key) {
    return peerSend(peer, std::format("\\m\\{}\\xfer\\{} {} {}", type, key.profile, key.count, key.time));
}

// 0x0057BF60: and ended: "\len\<n>\msg\", a line's end, the text, a 0; the peer's timeout 300 s on.
Result Connection::finishTransferMessage(Peer& peer, std::string_view text) {
    Result r = peerSend(peer, std::format("\\len\\{}\\msg\\\n", text.size()));
    if (r == Result::NoError) r = peerSend(peer, text);
    if (r == Result::NoError) r = peerSend(peer, std::string_view("\0", 1));
    if (r != Result::NoError) return r;
    peer.timeout = now() + 300;
    return Result::NoError;
}

// ---- The transfers.

int32_t Connection::addSendingTransfer(int32_t profileId, std::string_view message,
                                       const std::vector<std::pair<std::string, std::string>>& files) {
    Transfer& t = createTransfer(profileId, true);
    t.message = std::string(message);
    for (const auto& [name, path] : files) addTransferFile(t, name, &path);
    return t.id;
}

// 0x00576F80: a transfer at the list's end (its key zeros): the next id (+0x484), the profile, its side; waiting for
// the pong (0), no file yet (-1), not throttled (-1).
Connection::Transfer& Connection::createTransfer(int32_t profileId, bool sender) {
    auto t = std::make_unique<Transfer>();
    t->id = m_nextTransferId++;
    t->sender = sender;
    t->profile = profileId;
    m_transfers.push_back(std::move(t));
    return *m_transfers.back();
}

// 0x00577040: a transfer asked of us: waiting (1), its key the sender's, its base directory the current one (without
// a separator at its end).
Connection::Transfer& Connection::addReceivingTransfer(const TransferKey& key, int32_t profileId) {
    Transfer& t = createTransfer(profileId, false);
    t.state = 1;
    t.key = key;
    char directory[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, directory) != 0) t.baseDirectory = directory;
    return t;
}

// 0x005770D0: found by its id (a binary search: the ids only grow along the list) and let go (0x00576EA0).
void Connection::removeTransfer(const Transfer* transfer) {
    const auto it =
        std::find_if(m_transfers.begin(), m_transfers.end(), [transfer](const auto& t) { return t.get() == transfer; });
    if (it != m_transfers.end()) m_transfers.erase(it);
}

// 0x0057C090 with the transfer callback (+0x1E8; type 7), queued set or not.
void Connection::transferEvent(const Transfer& t, int32_t type, int32_t index, int32_t num, std::u16string message) {
    const TransferArg arg{t.id, type, index, num, std::move(message)};
    addCallback(7, 0, [this, arg] {
        if (onTransfer) onTransfer(arg);
    });
}

// 0x00577190: a file at the transfer's end: its path (a sender's), its name with each '\' made '/', a directory when
// the name as given ends with '/' (Stock problem: one ending with '\' is not), its size -1. Its wide name and path are
// made after it is in the list, and lost.
Connection::TransferFile& Connection::addTransferFile(Transfer& t, const std::string& name, const std::string* path) {
    TransferFile f;
    if (path != nullptr) f.path = *path;
    f.name = slashes(name);
    if (!name.empty() && name.back() == '/') f.flags = 2;
    t.files.push_back(std::move(f));
    return t.files.back();
}

// 0x005772F0: the request (200): \version\1\num\<files>, each file's \name<i>\ \size<i>\ \mtime<i>\, the
// transfer's message as the text.
Result Connection::sendTransferRequest(Transfer& t) {
    Peer& peer = *t.peer;
    Result r = startTransferMessage(peer, kSendRequest, t.key);
    if (r != Result::NoError) return r;
    r = peerSend(peer, std::format("\\version\\{}\\num\\{}", 1, t.files.size()));
    if (r != Result::NoError) return r;
    for (size_t i = 0; i < t.files.size(); ++i) {
        const TransferFile& f = t.files[i];
        for (const std::string& part :
             {std::format("\\name{}\\", i), f.name, std::format("\\size{}\\", i), std::format("{}", f.size),
              std::format("\\mtime{}\\", i), std::format("{}", f.modTime)}) {
            r = peerSend(peer, part);
            if (r != Result::NoError) return r;
        }
    }
    r = finishTransferMessage(peer, cString(t.message));
    t.lastSend = GetTickCount();
    return r;
}

// 0x005774E0: the first transfer of the key.
Connection::Transfer* Connection::findTransfer(const TransferKey& key) {
    for (const auto& t : m_transfers) {
        if (t->key == key) return t.get();
    }
    return nullptr;
}

// 0x00577540: "\file\<file>\reason\<reason>" (Stock problem: under the type 0x80E, the event's GP_TRANSFER_FILE_SKIP,
// not the skip's 205, so the other side's dispatch never takes it).
void Connection::sendFileReason(Transfer& t, int32_t file, int32_t reason) {
    if (t.peer == nullptr) return;
    if (startTransferMessage(*t.peer, kFileSkip, t.key) != Result::NoError) return;
    peerSend(*t.peer, std::format("\\file\\{}\\reason\\{}", file, reason));
    finishTransferMessage(*t.peer, "");
    t.lastSend = GetTickCount();
}

// 0x005775E0: the current file's reason sent, the next file.
void Connection::failCurrentFile(Transfer& t, int32_t reason) {
    sendFileReason(t, t.currentFile, reason);
    ++t.currentFile;
}

// 0x00577600: a peer's request (200), taken only with the transfer callback set: \version\ at least 1, \num\ files at
// least 1, each \name<i>\ (259 characters, not with "//" or "./", not ending '.', not beginning '/', none of
// ":*?\"<>|" or a line's end), \size<i>\ not under 0 and \mtime<i>\: a transfer waiting for our answer (its files
// added, their sizes the total), its event with the files' count and the text. Anything wrong: false, the transfer
// gone.
bool Connection::processTransferRequest(Peer& peer, const TransferKey& key, const std::string& header,
                                        const std::string& text) {
    if (!onTransfer) return false;
    std::string value;
    if (!valueForKey(header, "\\version\\", value, 0x10) || atolOf(value) < 1) return false;
    if (!valueForKey(header, "\\num\\", value, 0x10)) return false;
    const int32_t count = atolOf(value);
    if (count < 1) return false;
    Transfer& t = addReceivingTransfer(key, peer.profile);
    t.peer = &peer;
    auto fail = [this, &t] {
        removeTransfer(&t);
        return false;
    };
    int32_t total = 0;
    for (int32_t i = 0; i < count; ++i) {
        std::string name;
        if (!valueForKey(header, std::format("\\name{}\\", i), name, 0x104)) return fail();
        if (name.find("//") != std::string::npos || name.find("./") != std::string::npos) return fail();
        if (!name.empty() && (name.back() == '.' || name.front() == '/')) return fail();
        if (name.find_first_of(":*?\"<>|\n") != std::string::npos) return fail();
        if (!valueForKey(header, std::format("\\size{}\\", i), value, 0x10)) return fail();
        const int32_t size = atolOf(value);
        if (size < 0) return fail();
        total += size;
        if (!valueForKey(header, std::format("\\mtime{}\\", i), value, 0x10)) return fail();
        const auto modTime = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        TransferFile& f = addTransferFile(t, name, nullptr);
        f.size = size;
        f.modTime = modTime;
    }
    transferEvent(t, kTransferSendRequest, 0, count, fromUtf8(text));
    t.totalSize = total;
    return true;
}

// 0x00577910: the reply to our request (201): \version\ at least 1 and \result\: 0 accepted (transferring from the
// first file), 1 rejected, else not accepting; the event with the text.
bool Connection::processTransferReply(Transfer& t, const std::string& header, const std::string& text) {
    if (!t.sender) return false;
    std::string value;
    if (!valueForKey(header, "\\version\\", value, 0x10) || atolOf(value) < 1) return false;
    if (!valueForKey(header, "\\result\\", value, 0x10)) return false;
    const int32_t result = atolOf(value);
    const int32_t type = result == 0 ? kTransferAccepted : result == 1 ? kTransferRejected : kTransferNotAccepting;
    transferEvent(t, type, 0, 0, fromUtf8(text));
    if (result == 0) {
        t.state = 2;
        t.currentFile = 0;
    }
    return true;
}

// 0x00577A50: a file begun (202) on our side receiving: \file\ the current one, not a directory; \size\ (not under 0;
// the total made right) and \mtime\; its MD5 begun and a temporary file made, "<base>gpt_<id>_<file>_<rand()>.gpt"
// (up to five tries): its event, or failed (a write error: its reason sent, the next file).
bool Connection::processFileBegin(Transfer& t, const std::string& header) {
    if (t.sender) return false;
    std::string value;
    if (!valueForKey(header, "\\file\\", value, 0x10)) return false;
    const int32_t index = atolOf(value);
    if (index < 0 || index >= static_cast<int32_t>(t.files.size()) || index != t.currentFile) return false;
    TransferFile& f = t.files[static_cast<size_t>(index)];
    if ((f.flags & 2) != 0) return false;
    if (!valueForKey(header, "\\size\\", value, 0x10)) return false;
    const int32_t size = atolOf(value);
    if (size < 0) return false;
    t.totalSize += size - f.size;
    if (!valueForKey(header, "\\mtime\\", value, 0x10)) return false;
    const auto modTime = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    f.md5 = Md5Context{};
    f.modTime = modTime;
    f.size = size;
    for (int tries = 0; tries < 5; ++tries) {
        const std::string temporary =
            std::format("{}gpt_{}_{}_{}.gpt", t.baseDirectory, t.id, index, gameRandom().next());
        f.file = _fsopen(temporary.c_str(), "wb", _SH_DENYNO);  // fopen's sharing
        if (f.file != nullptr) {
            f.path = temporary;
            break;
        }
    }
    transferEvent(t, f.file != nullptr ? kFileBegin : kFileFailed, index, f.file != nullptr ? 0 : kFileWriteError);
    if (f.file == nullptr) {
        failCurrentFile(t, 1);
        f.flags |= 8;
        f.reason = kFileWriteError;
    }
    return true;
}

// 0x00577CD0: a file ended (203) on our side receiving (another file's is let be): a directory done; else \md5\
// against the file's own (a difference failing it, the file removed), the file closed. Its event (a directory's, a
// failure's data error, else the end), the next file; past the last, done.
bool Connection::processFileEnd(Transfer& t, const std::string& header) {
    if (t.currentFile == -1) return false;
    std::string value;
    if (!valueForKey(header, "\\file\\", value, 0x10)) return false;
    const int32_t index = atolOf(value);
    if (index < 0 || index >= static_cast<int32_t>(t.files.size())) return false;
    if (index != t.currentFile) return true;
    TransferFile& f = t.files[static_cast<size_t>(t.currentFile)];
    if (t.sender) return true;
    bool bad = false;
    if ((f.flags & 2) != 0) {
        f.flags |= 0x10;
    } else {
        if (f.file == nullptr) return false;
        std::string md5;
        if (!valueForKey(header, "\\md5\\", md5, 0x21)) return false;
        bad = f.md5.finalHex() != md5;
        if (bad) {
            f.flags |= 8;
            f.reason = kFileDataError;
        } else {
            f.flags |= 0x10;
        }
        std::fclose(f.file);
        f.file = nullptr;
        if (bad) std::remove(f.path.c_str());
    }
    if ((f.flags & 2) != 0) {
        transferEvent(t, kFileDirectory, t.currentFile, 0);
    } else {
        transferEvent(t, bad ? kFileFailed : kFileEnd, t.currentFile, bad ? kFileDataError : 0);
    }
    ++t.currentFile;
    if (t.currentFile == static_cast<int32_t>(t.files.size())) {
        t.state = 3;
        transferEvent(t, kTransferDone, 0, 0);
    }
    return true;
}

// 0x00577F50: a file's bytes (204) for the current file (another's are let be): written (a short write failing it:
// the file closed and removed, its reason sent, the next file), its MD5 and the progresses; its event.
bool Connection::processFileData(Transfer& t, const std::string& header, std::string_view text) {
    if (t.currentFile == -1) return false;
    std::string value;
    if (!valueForKey(header, "\\file\\", value, 0x10)) return false;
    const int32_t index = atolOf(value);
    if (index < 0 || index >= static_cast<int32_t>(t.files.size())) return false;
    if (index != t.currentFile) return true;
    TransferFile& f = t.files[static_cast<size_t>(t.currentFile)];
    if ((f.flags & 2) != 0 || f.file == nullptr) return false;
    const bool bad = std::fwrite(text.data(), 1, text.size(), f.file) != text.size();
    if (bad) {
        f.flags |= 8;
        f.reason = kFileWriteError;
        std::fclose(f.file);
        f.file = nullptr;
        std::remove(f.path.c_str());
    } else {
        f.md5.update(text.data(), text.size());
        f.progress += static_cast<int32_t>(text.size());
        t.progress += static_cast<int32_t>(text.size());
    }
    transferEvent(t, bad ? kFileFailed : kFileProgress, t.currentFile, bad ? kFileWriteError : f.progress);
    if (bad) failCurrentFile(t, 1);
    return true;
}

// 0x00578120: a file skipped or failed by the other side (205): \file\ and \reason\ (2 skipped, 0 a read error, else
// a write error). The current one: closed and removed when receiving, the next file, its event; a later one: marked
// to skip or failed; an earlier one: false.
bool Connection::processFileSkip(Transfer& t, const std::string& header) {
    std::string value;
    if (!valueForKey(header, "\\file\\", value, 0x10)) return false;
    const int32_t index = atolOf(value);
    if (index < 0 || index >= static_cast<int32_t>(t.files.size())) return false;
    TransferFile& f = t.files[static_cast<size_t>(index)];
    if (!valueForKey(header, "\\reason\\", value, 0x10)) return false;
    const int32_t reason = atolOf(value);
    const int32_t error = reason != 0 ? kFileWriteError : kFileReadError;
    if (index == t.currentFile) {
        if (!t.sender && f.file != nullptr) {
            std::fclose(f.file);
            f.file = nullptr;
            std::remove(f.path.c_str());
        }
        ++t.currentFile;
        transferEvent(t, reason == 2 ? kFileSkip : kFileFailed, index, reason == 2 ? 0 : error);
        return true;
    }
    if (index < t.currentFile) return false;
    if (reason == 2) {
        f.flags |= 4;
    } else {
        f.flags |= 8;
        f.reason = error;
    }
    return true;
}

// 0x005782D0: the rate (206): \rate\ taken (0 pauses our sending); our side sending says it back; its event.
bool Connection::processThrottle(Transfer& t, const std::string& header) {
    std::string value;
    if (!valueForKey(header, "\\rate\\", value, 0x10)) return false;
    const int32_t rate = atolOf(value);
    t.throttle = rate;
    if (t.sender) {
        if (startTransferMessage(*t.peer, kThrottle, t.key) != Result::NoError) return false;
        peerSend(*t.peer, "\\rate\\");
        peerSend(*t.peer, std::format("{}", rate));
        finishTransferMessage(*t.peer, "");
        t.lastSend = GetTickCount();
    }
    transferEvent(t, kTransferThrottle, 0, rate);
    return true;
}

// 0x005783F0: cancelled by the other side (207): its event.
bool Connection::processCancel(Transfer& t) {
    t.state = 4;
    transferEvent(t, kTransferCancelled, 0, 0);
    return true;
}

// 0x00578450: our file's end (203): \file\<current>, a file's \md5\ (its MD5 ended; none for a directory).
Result Connection::sendFileEnd(Transfer& t, TransferFile& f) {
    Peer& peer = *t.peer;
    const Result r = startTransferMessage(peer, kEnd, t.key);
    if (r != Result::NoError) return r;
    peerSend(peer, "\\file\\");
    peerSend(peer, std::format("{}", t.currentFile));
    if ((f.flags & 2) == 0) {
        const std::string md5 = f.md5.finalHex();
        peerSend(peer, "\\md5\\");
        peerSend(peer, md5);
    }
    finishTransferMessage(peer, "");
    t.lastSend = GetTickCount();
    return Result::NoError;
}

// 0x00578530: our file begun (202): its size and time from the open file (_fstat; failing "Can't get info on file.",
// the parameter error), \file\<current>\size\<size>\mtime\<time>.
Result Connection::sendFileBegin(Transfer& t, TransferFile& f) {
    struct _stat64 st {};
    if (_fstat64(_fileno(f.file), &st) != 0) {
        setError("Can't get info on file.");
        return Result::ParameterError;
    }
    f.size = static_cast<int32_t>(st.st_size);
    f.modTime = static_cast<uint32_t>(st.st_mtime);
    const Result r = startTransferMessage(*t.peer, kBegin, t.key);
    if (r != Result::NoError) return r;
    peerSend(*t.peer, std::format("\\file\\{}\\size\\{}\\mtime\\{}", t.currentFile, f.size, f.modTime));
    finishTransferMessage(*t.peer, "");
    t.lastSend = GetTickCount();
    return Result::NoError;
}

// 0x00578640: our file's bytes (204): \file\<current>, the bytes as the text.
Result Connection::sendFileData(Transfer& t, std::string_view bytes) {
    const Result r = startTransferMessage(*t.peer, kData, t.key);
    if (r != Result::NoError) return r;
    peerSend(*t.peer, "\\file\\");
    peerSend(*t.peer, std::format("{}", t.currentFile));
    finishTransferMessage(*t.peer, bytes);
    t.lastSend = GetTickCount();
    return Result::NoError;
}

// 0x005786A0: our current file. One to skip: its reason 2 sent, the next file and its skip's event (Stock problem:
// naming the file after it). A directory: its event, its end, done, the next file. Not yet open: opened (failing: its
// event of a read error, its reason 0 sent, failed, the next file), begun, its MD5 begun and its event. Then 8 KB at a
// time while the peer's output is empty: its MD5, its bytes sent, the progresses and its event; read short before its
// size: its reason 0 sent, failed, the next file (no event). All of it read: closed, its end, its event, done, the
// next file.
Result Connection::sendCurrentFile(Transfer& t) {
    TransferFile& f = t.files[static_cast<size_t>(t.currentFile)];
    if ((f.flags & 4) != 0) {
        sendFileReason(t, t.currentFile, 2);
        ++t.currentFile;
        transferEvent(t, kFileSkip, t.currentFile, 0);
        return Result::NoError;
    }
    if ((f.flags & 2) != 0) {
        transferEvent(t, kFileDirectory, t.currentFile, 0);
        sendFileEnd(t, f);
        f.flags |= 0x10;
        ++t.currentFile;
        return Result::NoError;
    }
    if (f.file == nullptr) {
        f.file = _fsopen(f.path.c_str(), "rb", _SH_DENYNO);
        if (f.file == nullptr) {
            transferEvent(t, kFileFailed, t.currentFile, kFileReadError);
            sendFileReason(t, t.currentFile, 0);
            ++t.currentFile;
            f.flags |= 8;
            f.reason = kFileReadError;
            return Result::NoError;
        }
        const Result r = sendFileBegin(t, f);
        if (r != Result::NoError) return r;
        f.md5 = Md5Context{};
        transferEvent(t, kFileBegin, t.currentFile, 0);
    }
    while (f.progress < f.size && outputLength(*t.peer) == 0) {
        const size_t read = std::fread(g_fileBuffer, 1, sizeof g_fileBuffer, f.file);
        if (read != 0) {
            f.md5.update(g_fileBuffer, read);
            const Result r = sendFileData(t, std::string_view(g_fileBuffer, read));
            if (r != Result::NoError) return r;
            t.progress += static_cast<int32_t>(read);
            f.progress += static_cast<int32_t>(read);
            transferEvent(t, kFileProgress, t.currentFile, f.progress);
        }
        if (read < sizeof g_fileBuffer && f.progress != f.size) {
            sendFileReason(t, t.currentFile, 0);
            ++t.currentFile;
            f.flags |= 8;
            f.reason = kFileReadError;
            return Result::NoError;
        }
    }
    if (f.progress != f.size) return Result::NoError;
    std::fclose(f.file);
    f.file = nullptr;
    sendFileEnd(t, f);
    transferEvent(t, kFileEnd, t.currentFile, 0);
    f.flags |= 0x10;
    ++t.currentFile;
    return Result::NoError;
}

// 0x00578A70: a transfer of ours not yet over. No peer after 60 s: no connection (its event); a peer, after 4
// minutes silent the keep-alive (208). Transferring, not paused, the peer's messages all gone: the files in turn while
// each is done at once; past the last: done (its event).
Result Connection::processTransfer(Transfer& t) {
    if (!t.sender || t.state >= 3) return Result::NoError;
    const uint32_t silent = GetTickCount() - t.lastSend;
    if (t.peer == nullptr) {
        if (silent > 60000) {
            t.state = 5;
            transferEvent(t, kTransferNoConnection, 0, 0);
            return Result::NoError;
        }
    } else if (silent > 240000) {
        const Result r = startTransferMessage(*t.peer, kKeepAlive, t.key);
        if (r != Result::NoError) return r;
        finishTransferMessage(*t.peer, "");
        t.lastSend = GetTickCount();
    }
    if (t.throttle == 0 || t.state < 2 || t.peer == nullptr || !t.peer->messages.empty()) return Result::NoError;
    const auto count = static_cast<int32_t>(t.files.size());
    while (t.currentFile < count) {
        const int32_t before = t.currentFile;
        const Result r = sendCurrentFile(t);
        if (r != Result::NoError) return r;
        if (t.currentFile == before) break;
    }
    if (t.currentFile == count) {
        transferEvent(t, kTransferDone, 0, 0);
        t.state = 3;
    }
    return Result::NoError;
}

// 0x00578C10: each transfer (its errors end nothing).
Result Connection::processTransfers() {
    const size_t count = m_transfers.size();
    for (size_t i = 0; i < count; ++i) processTransfer(*m_transfers[i]);
    return Result::NoError;
}

// 0x00578C60: every transfer's connection lost: its event, no connection (5).
void Connection::transfersLost() {
    for (const auto& t : m_transfers) {
        transferEvent(*t, kTransferLostConnection, 0, 0);
        t->state = 5;
    }
}

// 0x00578D00: the profile's pong to the transfers of ours waiting for it: through the server, no connection (its
// event); through a peer, their request sent to it and waiting for the reply.
void Connection::transfersPong(int32_t profileId, Peer* peer) {
    const size_t count = m_transfers.size();
    for (size_t i = 0; i < count; ++i) {
        Transfer& t = *m_transfers[i];
        if (t.state != 0 || t.profile != profileId) continue;
        if (peer == nullptr) {
            t.state = 5;
            transferEvent(t, kTransferNoConnection, 0, 0);
        } else {
            t.peer = peer;
            sendTransferRequest(t);
            t.state = 1;
        }
    }
}

// 0x00578DC0: the reply to a request (201): \version\1\result\<result>, the message as the text.
void Connection::sendTransferReply(Peer& peer, const TransferKey& key, int32_t result, std::string_view message) {
    if (startTransferMessage(peer, kSendReply, key) != Result::NoError) return;
    if (peerSend(peer, std::format("\\version\\{}\\result\\{}", 1, result)) != Result::NoError) return;
    finishTransferMessage(peer, message);
}

// 0x00578E60: a transfer's message from a peer: \xfer\ "<profile> <count> <time>" (else nothing). A request (200)
// not taken is answered "not accepting" (2). Else the transfer of the key with this peer (none: nothing): 201 the
// reply, 202 a file begun, 203 ended, 204 its bytes, 205 skipped, 206 the rate, 207 cancelled, 208 the keep-alive
// (nothing); one that fails is the transfer's error event.
void Connection::processTransferMessage(Peer& peer, int32_t type, const std::string& header, const std::string& text) {
    std::string value;
    if (!valueForKey(header, "\\xfer\\", value, 0x40)) return;
    TransferKey key;
    if (sscanf_s(value.c_str(), "%d %u %u", &key.profile, &key.count, &key.time) != 3) return;
    if (type == kSendRequest) {
        if (!processTransferRequest(peer, key, header, text)) sendTransferReply(peer, key, 2, "");
        return;
    }
    Transfer* t = findTransfer(key);
    if (t == nullptr || t->peer != &peer) return;
    bool done = false;
    switch (type) {
    case kSendReply: done = processTransferReply(*t, header, text); break;
    case kBegin: done = processFileBegin(*t, header); break;
    case kEnd: done = processFileEnd(*t, header); break;
    case kData: done = processFileData(*t, header, text); break;
    case kSkip: done = processFileSkip(*t, header); break;
    case kThrottle: done = processThrottle(*t, header); break;
    case kCancel: done = processCancel(*t); break;
    default: return;
    }
    if (!done) transferEvent(*t, kTransferError, 0, 0);  // 0x00577130
}

}  // namespace oj::net::gp
