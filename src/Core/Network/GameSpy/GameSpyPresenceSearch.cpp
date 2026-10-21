// GameSpy Presence's search manager (gpsp.gamespy.com:29901, 0x00571500 .. 0x00573170) and the answers of the
// operations nothing of the executable makes (a new profile, a unique nick) and the profile's updates: the SDK's code
// the linker kept, reached by none of the game's calls (docs/spec/28-network.md, "GameSpy Presence").

#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyPresenceDetail.h"

#include "Core/Network/GameSpy/GameSpy.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace oj::net::gp {

using namespace detail;

namespace {

constexpr std::string_view kFinalKey = "\\final\\";

}  // namespace

bool Search::hasCallback() const {
    switch (type) {
    case 1: return static_cast<bool>(onProfiles);
    case 2: return static_cast<bool>(onValidEmail);
    case 3: return static_cast<bool>(onNicks);
    case 4: return static_cast<bool>(onPlayers);
    case 5:
    case 6: return static_cast<bool>(onProfile);
    case 7: return static_cast<bool>(onOthers);
    case 8: return static_cast<bool>(onSuggested);
    default: return false;
    }
}

Result Connection::startSearch(Search search, bool blocking) {
    ++m_searchCount;
    Operation& op = addOperation(3, blocking);
    op.search = std::make_unique<SearchState>();
    op.search->search = std::move(search);
    const int32_t id = op.id;
    Result r = connectSearchManager(op);
    if (r != Result::NoError) return r;
    if (blocking) r = processInternal(id);
    return r;
}

Result Connection::profileSearch(std::string_view nick, std::string_view uniquenick, std::string_view email,
                                 std::string_view firstname, std::string_view lastname, int32_t icquin, int32_t skip,
                                 bool blocking, std::function<void(ProfileSearchResponse&)> callback) {
    if (nick.empty() && email.empty() && firstname.empty() && lastname.empty() && icquin == 0 && uniquenick.empty()) {
        setError("No search criteria.");
        return Result::ParameterError;
    }
    Search s;
    s.type = 1;
    s.nick = cut(nick, 0x1F);
    s.uniquenick = cut(uniquenick, 0x33);
    s.email = cut(email, 0x33);
    for (char& c : s.email) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    s.firstname = cut(firstname, 0x1F);
    s.lastname = cut(lastname, 0x1F);
    s.icquin = icquin;
    s.skip = skip < 0 ? 0 : skip;
    s.onProfiles = std::move(callback);
    return startSearch(std::move(s), blocking);
}

// A non-blocking socket to the search manager; the operation connecting (1). The socket, its non-blocking mode, the
// name or the connect failing: the network error (5, fatal).
Result Connection::connectSearchManager(Operation& op) {
    const auto fail = [this](std::string_view text) {
        setError(text, kNetwork);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    const uint32_t ip = resolveHost(m_server.searchHost);
    if (ip == 0) return fail("Could not resolve search mananger host name.");
    if (!op.search->socket.connect(ip, m_server.searchPort)) return fail("There was an error creating a socket.");
    op.state = 1;
    return Result::NoError;
}

Result Connection::readPair(const std::string& text, size_t& index, std::string& key, std::string& value) {
    const auto at = [&](size_t i) { return i < text.size() ? text[i] : '\0'; };
    const auto parseError = [this] {
        setError("Parse Error.", kParse);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    size_t i = index;
    if (at(i) != '\\') return parseError();
    ++i;
    key.clear();
    for (;;) {
        const char c = at(i);
        ++i;
        if (c == '\\') break;
        if (c == '\0' || key.size() == 0x1FF) return parseError();
        key.push_back(c);
    }
    value.clear();
    for (;;) {
        const char c = at(i);
        if (c == '\\' || c == '\0') break;
        if (value.size() == 0x1FF) return parseError();
        value.push_back(c);
        ++i;
    }
    index = i;
    return Result::NoError;
}

void Connection::searchError(std::string_view text) {
    setError(text, kParse);
    callError(Result::NetworkError, true);
}

// The searches not answered and not taken by an outer round are marked and processed (a result ends one: the failed
// callback); then those answered go (their sockets closed).
Result Connection::processSearches() {
    if (m_searchCount <= 0) return Result::NoError;
    std::vector<Operation*> round;
    for (const auto& op : m_operations) {
        if (op->type == 3 && op->state != 5 && op->search->processing == 0) {
            round.push_back(op.get());
            op->search->processing = 1;
        }
    }
    const auto alive = [this](const Operation* op) {
        for (const auto& o : m_operations) {
            if (o.get() == op) return true;
        }
        return false;
    };
    for (Operation* op : round) {
        if (!alive(op)) continue;
        const Result r = processSearch(*op);
        if (r != Result::NoError && alive(op)) op->result = r;
    }
    for (Operation* op : round) {
        if (!alive(op)) continue;
        op->search->processing = 0;
        if (op->search->done != 0) {
            --m_searchCount;
            op->search->socket.close();
            removeOperation(op);
        }
    }
    return Result::NoError;
}

// gpiProcessSearch (0x00571A80): the bytes waiting sent ("SM"); connecting, the connect's progress: refused, "Could not
// connect to the search manager." (0x0D01) and the server error; made, the request by the kind, then \gamename\ and
// \final\, waiting (4). Waiting, the bytes come (a socket's error: "There was an error reading from the server.",
// 0x0D01, the network error); with \final\ answered (5): an \error\ ends it (its callback); else the answer by the
// kind (below), the search done. A blocking search loops here (Sleep(10)) until then.
Result Connection::processSearch(Operation& op) {
    SearchState& st = *op.search;
    const Search& s = st.search;
    const bool blocking = op.blocking;
    const auto put = [&st](std::string_view text) { st.output.append(text); };
    const auto putInt = [&st](int64_t n) { st.output.append(std::to_string(n)); };
    for (;;) {
        if (!st.output.empty()) {
            st.socket.send(st.output);
            st.output.clear();
        }
        st.socket.poll();
        if (op.state == 1) {
            if (st.socket.state() == TcpConnection::State::Failed) {
                setError("Could not connect to the search manager.", 0x0D01);
                callError(Result::ServerError, false);
                return Result::ServerError;
            }
            if (st.socket.connected()) {
                switch (s.type) {
                case 1:
                    put("\\search\\");
                    put("\\sesskey\\");
                    putInt(m_sessKey);
                    put("\\profileid\\");
                    putInt(m_profileId);
                    put("\\namespaceid\\");
                    putInt(m_namespaceId);
                    if (!s.nick.empty()) {
                        put("\\nick\\");
                        put(s.nick);
                    }
                    if (!s.uniquenick.empty()) {
                        put("\\uniquenick\\");
                        put(s.uniquenick);
                    }
                    if (!s.email.empty()) {
                        put("\\email\\");
                        put(s.email);
                    }
                    if (!s.firstname.empty()) {
                        put("\\firstname\\");
                        put(s.firstname);
                    }
                    if (!s.lastname.empty()) {
                        put("\\lastname\\");
                        put(s.lastname);
                    }
                    if (s.icquin != 0) {
                        put("\\icquin\\");
                        putInt(s.icquin);
                    }
                    if (s.skip > 0) {
                        put("\\skip\\");
                        putInt(s.skip);
                    }
                    break;
                case 2:
                    put("\\valid\\");
                    put("\\email\\");
                    put(s.email);
                    break;
                case 3:
                    put("\\nicks\\");
                    put("\\email\\");
                    put(s.email);
                    put("\\pass\\");
                    put(s.password);
                    put("\\namespaceid\\");
                    putInt(m_namespaceId);
                    break;
                case 4:
                    put("\\pmatch\\");
                    put("\\sesskey\\");
                    putInt(m_sessKey);
                    put("\\profileid\\");
                    putInt(m_profileId);
                    put("\\productid\\");
                    putInt(s.productId);
                    break;
                case 5:
                    put("\\check\\");
                    put("\\nick\\");
                    put(s.nick);
                    put("\\email\\");
                    put(s.email);
                    put("\\pass\\");
                    put(s.password);
                    break;
                case 6:
                    put("\\newuser\\");
                    put("\\nick\\");
                    put(s.nick);
                    put("\\email\\");
                    put(s.email);
                    put("\\pass\\");
                    put(s.password);
                    put("\\productID\\");
                    putInt(m_productId);
                    put("\\namespaceid\\");
                    putInt(m_namespaceId);
                    put("\\uniquenick\\");
                    put(s.uniquenick);
                    if (!s.cdkey.empty()) {
                        put("\\cdkey\\");
                        put(s.cdkey);
                    }
                    break;
                case 7:
                    put("\\others\\");
                    put("\\sesskey\\");
                    putInt(m_sessKey);
                    put("\\profileid\\");
                    putInt(m_profileId);
                    put("\\namespaceid\\");
                    putInt(m_namespaceId);
                    break;
                case 8:
                    put("\\uniquesearch\\");
                    put("\\preferrednick\\");
                    put(s.uniquenick);
                    put("\\namespaceid\\");
                    putInt(m_namespaceId);
                    break;
                default:
                    break;
                }
                put("\\gamename\\");
                put(availableGameName());
                put(kFinalKey);
                op.state = 4;
            }
        } else if (op.state == 4) {
            if (st.socket.state() == TcpConnection::State::Failed && !st.socket.lost()) {
                setError("There was an error reading from the server.", 0x0D01);
                callError(Result::NetworkError, false);
                return Result::NetworkError;
            }
            std::vector<uint8_t>& in = st.socket.received();
            if (!in.empty()) {
                st.input.append(reinterpret_cast<const char*>(in.data()), in.size());
                in.clear();
            }
            if (cString(st.input).find(kFinalKey) != std::string_view::npos) {
                op.state = 5;
                if (checkForError(st.input, true)) {
                    st.done = 1;
                    return Result::ServerError;
                }
                return searchAnswered(op);
            }
        }
        if (!blocking) return Result::NoError;
        Sleep(10);
    }
}

// The answer by the search's kind (0x005720E6 ..), read pair by pair (0x00579C70: a parse error ends it); a key not
// expected where one is: "Error reading from the search server." (1, fatal) and the network error.
//  1 \bsr\<profile> and its nick, uniquenick, firstname, lastname, email for each match (other keys passed over), then
//    \bsrdone\ and \more\ (not "0": more come); the callback at once (not queued); more coming and the callback leaving
//    it so, the next page searched (skip + the matches; the same callback). 2 \vr\ (not '0': valid). 3 \nr\, then
//    each \nick\ (a \uniquenick\ after one goes with it), \ndone\. 4 \psr\<profile> with its nick, status and
//    statuscode, \psrdone\. 5 \cur\ (not 0: the error's code, no profile; else the \pid\, needed). 6 \nur\ (not 0:
//    the error's code) and the \pid\ (needed when 0). 7 \others\, then \o\<profile> with its nick, uniquenick, first,
//    last, email, \odone\. 8 \us\<count>, each \nick\, \usdone\. Without a callback (all but 1) nothing is read.
Result Connection::searchAnswered(Operation& op) {
    SearchState& st = *op.search;
    const Search s = st.search;
    const std::string text = st.input;
    const int32_t id = op.id;
    size_t index = 0;
    std::string key;
    std::string value;
    const auto read = [&] { return readPair(text, index, key, value); };
    const auto bad = [this] {
        searchError("Error reading from the search server.");
        return Result::NetworkError;
    };
    const auto finish = [&st] {
        st.done = 1;
        return Result::NoError;
    };
    Result r = Result::NoError;
    switch (s.type) {
    case 1: {
        ProfileSearchResponse arg;
        arg.more = kDone;
        if ((r = read()) != Result::NoError) return r;
        for (;;) {
            if (key == "bsrdone") {
                if ((r = read()) != Result::NoError) return r;
                if (key == "more" && value != "0") arg.more = kMore;
                break;
            }
            if (key != "bsr") return bad();
            ++arg.numMatches;
            arg.matches.emplace_back();
            ProfileMatch& m = arg.matches.back();
            m.profile = static_cast<int32_t>(atolOf(value));
            for (;;) {
                const size_t before = index;
                if ((r = read()) != Result::NoError) return r;
                if (key == "nick") {
                    m.nick = fromUtf8Cut(value, 0x1F);
                } else if (key == "uniquenick") {
                    m.uniquenick = fromUtf8Cut(value, 0x33);
                } else if (key == "firstname") {
                    m.firstname = fromUtf8Cut(value, 0x1F);
                } else if (key == "lastname") {
                    m.lastname = fromUtf8Cut(value, 0x1F);
                } else if (key == "email") {
                    m.email = fromUtf8Cut(value, 0x33);
                } else if (key == "bsr" || key == "bsrdone") {
                    index = before;
                    break;
                }
            }
            if ((r = read()) != Result::NoError) return r;
        }
        const int32_t more = arg.more;
        if (s.onProfiles) s.onProfiles(arg);
        if (more == kMore && arg.more == kMore) {
            r = profileSearch(s.nick, s.uniquenick, s.email, s.firstname, s.lastname, s.icquin, s.skip + arg.numMatches,
                              op.blocking, s.onProfiles);
            if (r != Result::NoError) return r;
        }
        Operation* again = findOperation(id);
        if (again != nullptr && again->search) again->search->done = 1;
        return Result::NoError;
    }
    case 2: {
        if (!s.onValidEmail) return finish();
        if ((r = read()) != Result::NoError) return r;
        if (key != "vr") return bad();
        IsValidEmailResponse arg;
        arg.email = fromUtf8Cut(s.email, 0x33);
        arg.isValid = value.empty() || value[0] != '0' ? 1 : 0;
        auto cb = s.onValidEmail;
        addCallback(0, id, [cb, arg] { cb(arg); });
        return finish();
    }
    case 3: {
        if (!s.onNicks) return finish();
        UserNicksResponse arg;
        arg.email = fromUtf8Cut(s.email, 0x33);
        if ((r = read()) != Result::NoError) return r;
        if (key != "nr") return bad();
        for (;;) {
            if ((r = read()) != Result::NoError) return r;
            if (key == "nick") {
                arg.nicks.push_back(fromUtf8Cut(value, 0x1F));
                ++arg.numNicks;
            } else if (key == "uniquenick") {
                if (arg.numNicks <= 0) continue;
                arg.uniquenicks.resize(static_cast<size_t>(arg.numNicks));
                arg.uniquenicks[static_cast<size_t>(arg.numNicks - 1)] = fromUtf8Cut(value, 0x33);
            } else if (key == "ndone") {
                break;
            } else {
                return bad();
            }
        }
        auto cb = s.onNicks;
        addCallback(3, id, [cb, arg] { cb(arg); });
        return finish();
    }
    case 4: {
        if (!s.onPlayers) return finish();
        FindPlayersResponse arg;
        arg.productId = s.productId;
        if ((r = read()) != Result::NoError) return r;
        for (;;) {
            if (key == "psrdone") break;
            if (key != "psr") return bad();
            ++arg.numMatches;
            arg.matches.emplace_back();
            FoundPlayer& m = arg.matches.back();
            m.status = 1;
            m.profile = static_cast<int32_t>(atolOf(value));
            for (;;) {
                const size_t before = index;
                if ((r = read()) != Result::NoError) return r;
                if (key == "status") {
                    m.statusString = fromUtf8Cut(value, 0x100);
                } else if (key == "nick") {
                    m.nick = fromUtf8Cut(value, 0x1F);
                } else if (key == "statuscode") {
                    m.status = static_cast<int32_t>(atolOf(value));
                } else if (key == "psr" || key == "psrdone") {
                    index = before;
                    break;
                }
            }
            if ((r = read()) != Result::NoError) return r;
        }
        auto cb = s.onPlayers;
        addCallback(4, id, [cb, arg] { cb(arg); });
        return finish();
    }
    case 5:
    case 6: {
        if (!s.onProfile) return finish();
        if ((r = read()) != Result::NoError) return r;
        if (key != (s.type == 5 ? "cur" : "nur")) return bad();
        ProfileResponse arg;
        const auto code = static_cast<int32_t>(atolOf(value));
        arg.result = static_cast<Result>(code);
        if (code != 0) m_errorCode = code;
        std::string pid;
        if (s.type == 5 && code != 0) {
            arg.profile = 0;
        } else if (valueForKey(text, "\\pid\\", pid, 0x200)) {
            arg.profile = static_cast<int32_t>(atolOf(pid));
        } else if (code == 0) {
            return bad();
        }
        auto cb = s.onProfile;
        addCallback(0, id, [cb, arg] { cb(arg); });
        return finish();
    }
    case 7: {
        if (!s.onOthers) return finish();
        ReverseBuddiesResponse arg;
        if ((r = read()) != Result::NoError) return r;
        if (key != "others") return bad();
        if ((r = read()) != Result::NoError) return r;
        for (;;) {
            if (key == "odone") break;
            if (key != "o") return bad();
            ++arg.numProfiles;
            arg.profiles.emplace_back();
            ProfileMatch& m = arg.profiles.back();
            m.profile = static_cast<int32_t>(atolOf(value));
            for (;;) {
                const size_t before = index;
                if ((r = read()) != Result::NoError) return r;
                if (key == "nick") {
                    m.nick = fromUtf8Cut(value, 0x1F);
                } else if (key == "uniquenick") {
                    m.uniquenick = fromUtf8Cut(value, 0x33);
                } else if (key == "first") {
                    m.firstname = fromUtf8Cut(value, 0x1F);
                } else if (key == "last") {
                    m.lastname = fromUtf8Cut(value, 0x1F);
                } else if (key == "email") {
                    m.email = fromUtf8Cut(value, 0x33);
                } else if (key == "o" || key == "odone") {
                    index = before;
                    break;
                }
            }
            if ((r = read()) != Result::NoError) return r;
        }
        auto cb = s.onOthers;
        addCallback(8, id, [cb, arg] { cb(arg); });
        return finish();
    }
    case 8: {
        if (!s.onSuggested) return finish();
        if ((r = read()) != Result::NoError) return r;
        if (key != "us") return bad();
        // Stock problem: the count given sizes the list and the nicks are written on whatever their number.
        SuggestUniqueNickResponse arg;
        arg.numSuggestedNicks = static_cast<int32_t>(atolOf(value));
        for (;;) {
            if ((r = read()) != Result::NoError) return r;
            if (key == "nick") {
                arg.suggestedNicks.push_back(fromUtf8Cut(value, 0x33));
            } else if (key == "usdone") {
                break;
            } else {
                return bad();
            }
        }
        arg.numSuggestedNicks = static_cast<int32_t>(arg.suggestedNicks.size());
        auto cb = s.onSuggested;
        addCallback(9, id, [cb, arg] { cb(arg); });
        return finish();
    }
    default:
        return finish();
    }
}

// ---- The answers of the operations nothing makes.

int32_t Connection::addAnswerOperation(int32_t type, std::function<void(const ProfileResponse&)> onProfile,
                                       std::function<void(const ResultResponse&)> onResult) {
    Operation& op = addOperation(type, false);
    op.profileCallback = std::move(onProfile);
    op.resultCallback = std::move(onResult);
    return op.id;
}

// 0x00576A10: an \error\ ends it (its callback); a message not "\npr\" or without \profileid\: "Unexpected data was
// received from the server." (1, fatal), the network error; else the new profile to the callback and the operation
// gone.
Result Connection::processNewProfile(Operation& op, const std::string& message) {
    if (checkForError(message, true)) return Result::ServerError;
    const auto unexpected = [this] {
        setError("Unexpected data was received from the server.", kParse);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    };
    if (message.compare(0, 5, "\\npr\\") != 0) return unexpected();
    std::string value;
    if (!valueForKey(message, "\\profileid\\", value, 0x10)) return unexpected();
    ProfileResponse arg;
    arg.profile = static_cast<int32_t>(atolOf(value));
    if (op.profileCallback) {
        auto cb = op.profileCallback;
        addCallback(0, op.id, [cb, arg] { cb(arg); });
    }
    removeOperation(&op);
    return Result::NoError;
}

// 0x0057C3A0: the same for "\rn\" with no value.
Result Connection::processUniqueNick(Operation& op, const std::string& message) {
    if (checkForError(message, true)) return Result::ServerError;
    if (message.compare(0, 4, "\\rn\\") != 0) {
        setError("Unexpected data was received from the server.", kParse);
        callError(Result::NetworkError, true);
        return Result::NetworkError;
    }
    if (op.resultCallback) {
        auto cb = op.resultCallback;
        addCallback(0, op.id, [cb] { cb(ResultResponse{}); });
    }
    removeOperation(&op);
    return Result::NoError;
}

// 0x0057AAE0, each round of the connection manager first: the profile's updates gathered sent as
// \updatepro\\sesskey\<key><pairs>\final\ and the user's as \updateui\..., both emptied.
void Connection::sendInfoUpdates() {
    if (!m_updatePro.empty()) {
        m_output.append("\\updatepro\\\\sesskey\\");
        m_output.append(std::to_string(m_sessKey));
        m_output.append(m_updatePro);
        m_output.append(kFinalKey);
        m_updatePro.clear();
    }
    if (!m_updateUi.empty()) {
        m_output.append("\\updateui\\\\sesskey\\");
        m_output.append(std::to_string(m_sessKey));
        m_output.append(m_updateUi);
        m_output.append(kFinalKey);
        m_updateUi.clear();
    }
}

}  // namespace oj::net::gp
