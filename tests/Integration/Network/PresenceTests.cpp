// Integration tests: Presence.

#include "Framework/Test.h"
#include "Framework/Fakes/FakeGpcm.h"

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

using oj::test::check;
using oj::test::FakeGpcm;

}  // namespace

// GameSpy Presence against a stand-in of the connection manager on the loopback: the login (the response
// checked by the stand-in, the proof by the client), the profile's info, the status, a buddy's status message,
// a wrong password (the error 260), a proof that is not the server's (0x0108), a new user (\newuser\ and its
// password encoded, then the login with the ids of \nur\).
OJ_INTEGRATION_TEST(Presence, ConnectionManager) {
    namespace gp = oj::net::gp;
    oj::net::startWinsock();
    const std::filesystem::path info = std::filesystem::temp_directory_path() / "OpenJuice.Tests.gp.info";
    std::filesystem::remove(info);
    auto pump = [](const std::function<void()>& step, const std::function<bool()>& done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            step();
            Sleep(1);
        }
        return done();
    };
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::availableGameName() = "juicedpc";

    FakeGpcm fake;
    fake.nicks[1234] = "Tester";
    bool started = fake.start();
    const gp::Connection::Server server{"127.0.0.1", fake.port(), info.string()};
    std::shared_ptr<gp::Connection> c;
    started = started && gp::Connection::create(c, 0, 0, server) == gp::Result::NoError;
    int32_t loggedProfile = 0;
    gp::Result loggedResult = gp::Result::MemoryError;
    std::vector<gp::BuddyStatusArg> statuses;
    std::vector<gp::ErrorArg> errors;
    bool loggedIn = false;
    if (started) {
        c->onBuddyStatus = [&](const gp::BuddyStatusArg& a) { statuses.push_back(a); };
        c->onError = [&](const gp::ErrorArg& a) { errors.push_back(a); };
        c->connect(u"Tester", u"Tester@Example.COM", u"secret", true, false, [&](const gp::ConnectResponse& r) {
            loggedResult = r.result;
            loggedProfile = r.profile;
        });
        loggedIn = pump([&] { c->process(); }, [&] { return loggedResult != gp::Result::MemoryError; }) &&
                   loggedResult == gp::Result::NoError && loggedProfile == 1234 &&
                   c->state() == gp::State::LoggedIn && c->sessionKey() == 4242 && fake.loginChecked();
    }
    const std::string loginMessage = fake.message("\\login\\");
    const bool loginText = loginMessage.find("\\user\\Tester@tester@example.com\\response\\") != std::string::npos &&
                           loginMessage.ends_with("\\firewall\\1\\port\\0\\productid\\0\\gamename\\juicedpc"
                                                  "\\namespaceid\\0\\id\\1");
    std::u16string nick;
    bool infoOk = false;
    if (loggedIn) {
        c->getInfo(1234, true, true, [&](const gp::GetInfoResponse& r) { nick = r.nick; });
        infoOk = nick == u"Tester" && c->findProfile(1234) != nullptr && c->findProfile(1234)->cache &&
                 c->findProfile(1234)->cache->sex == 0x501;
        c->setStatus(1, u"Online", u"");
        c->setStatus(1, u"Online", u"");
        fake.queue("\\bm\\100\\f\\777\\msg\\|s|2|ss|Playing|ls|juicedpc://1.2.3.4:8667/?session=5|ip|16909060"
                   "|p|0\\final\\");
        pump([&] { c->process(); }, [&] { return !statuses.empty(); });
    }
    gp::BuddyStatus buddy;
    const bool buddyOk = statuses.size() == 1 && statuses[0].profile == 777 && statuses[0].index == 0 &&
                         c->getBuddyStatus(0, buddy) == gp::Result::NoError && buddy.status == 2 &&
                         buddy.statusString == u"Playing" &&
                         buddy.locationString == u"juicedpc://1.2.3.4:8667/?session=5" &&
                         buddy.ip == 0x04030201u;
    // The same status with its strings twice: sent once (the fixed build keeps the strings sent; the stock sent it
    // again).
    size_t statusSent = 0;
    pump([] {}, [&] {
        statusSent = 0;
        for (const std::string& m : fake.messages()) {
            if (m == "\\status\\1\\sesskey\\4242\\statstring\\Online\\locstring\\") ++statusSent;
        }
        return statusSent == 1;
    });
    Sleep(50);
    statusSent = 0;
    for (const std::string& m : fake.messages()) {
        if (m == "\\status\\1\\sesskey\\4242\\statstring\\Online\\locstring\\") ++statusSent;
    }
    // An error after the login: its code to the callback, fatal: disconnected.
    if (loggedIn) {
        fake.queue("\\error\\\\err\\6\\fatal\\\\errmsg\\Forced disconnect\\final\\");
        pump([&] { c->process(); }, [&] { return !errors.empty(); });
    }
    const bool errorOk = errors.size() == 1 && errors[0].errorCode == gp::kForcedDisconnect && errors[0].fatal &&
                         errors[0].errorString == u"Forced disconnect" && c->state() == gp::State::Disconnected;
    c.reset();
    std::ifstream infoFile(info);
    const std::string infoText((std::istreambuf_iterator<char>(infoFile)), std::istreambuf_iterator<char>());
    infoFile.close();
    const bool infoWritten = infoText.rfind("2\n[1234]\nuserid=5678\nvalid=1\nnick=Tester\n", 0) == 0 &&
                             infoText.find("sex=Female\n") != std::string::npos;
    fake.stop();
    check(started && loggedIn && loginText && infoOk && buddyOk && statusSent == 1 && errorOk && infoWritten,
          "GameSpy Presence: the login's proofs, the profile's info, the status, a buddy's status, an error, "
          "gp.info");

    // gp.info read back: the login carries the profile's ids; the cached info answers at once.
    FakeGpcm second;
    second.password = "wrong";
    bool wrongOk = second.start();
    std::shared_ptr<gp::Connection> w;
    wrongOk = wrongOk && gp::Connection::create(w, 0, 0, {"127.0.0.1", second.port(), info.string()}) ==
                             gp::Result::NoError;
    std::vector<gp::ErrorArg> wrongErrors;
    gp::Result wrongResult = gp::Result::NoError;
    bool answered = false;
    std::u16string cachedNick;
    if (wrongOk) {
        w->getInfo(1234, true, true, [&](const gp::GetInfoResponse& r) { cachedNick = r.nick; });
        w->onError = [&](const gp::ErrorArg& a) { wrongErrors.push_back(a); };
        w->connect(u"Tester", u"tester@example.com", u"secret", true, false, [&](const gp::ConnectResponse& r) {
            wrongResult = r.result;
            answered = true;
        });
        pump([&] { w->process(); }, [&] { return answered; });
    }
    const bool idsSent = second.message("\\login\\").find("\\userid\\5678\\profileid\\1234\\response\\") !=
                         std::string::npos;
    // "sex=Female" read back as Female (the fixed build; the stock's Pat).
    const bool femaleRead = w != nullptr && w->findProfile(1234) != nullptr && w->findProfile(1234)->cache &&
                            w->findProfile(1234)->cache->sex == 0x501;
    wrongOk = wrongOk && femaleRead && cachedNick == u"Tester" && idsSent && answered &&
              wrongResult == gp::Result::ServerError && wrongErrors.size() == 1 &&
              wrongErrors[0].errorCode == gp::kLoginBadPassword && wrongErrors[0].fatal &&
              w->state() == gp::State::Disconnected;
    w.reset();
    second.stop();
    check(wrongOk, "GameSpy Presence: gp.info read back (the login's ids, the info cached), a wrong password");

    // The server's proof not right: 0x0108. A new user: \newuser\ with the password encoded, \nur\'s ids in the
    // login.
    std::filesystem::remove(info);
    FakeGpcm proof;
    proof.badProof = true;
    bool proofOk = proof.start();
    std::shared_ptr<gp::Connection> p;
    proofOk = proofOk && gp::Connection::create(p, 0, 0, {"127.0.0.1", proof.port(), info.string()}) ==
                             gp::Result::NoError;
    std::vector<gp::ErrorArg> proofErrors;
    if (proofOk) {
        p->onError = [&](const gp::ErrorArg& a) { proofErrors.push_back(a); };
        p->connect(u"Tester", u"tester@example.com", u"secret", true, false, [](const gp::ConnectResponse&) {});
        pump([&] { p->process(); }, [&] { return !proofErrors.empty(); });
    }
    proofOk = proofOk && proofErrors.size() == 1 && proofErrors[0].errorCode == gp::kLoginServerAuthFailed;
    p.reset();
    proof.stop();

    FakeGpcm fresh;
    fresh.userId = 99;
    fresh.profileId = 98;
    bool newOk = fresh.start();
    std::shared_ptr<gp::Connection> n;
    newOk = newOk && gp::Connection::create(n, 0, 0, {"127.0.0.1", fresh.port(), info.string()}) ==
                         gp::Result::NoError;
    int32_t newProfile = 0;
    if (newOk) {
        n->connectNewUser(u"Newbie", u"", u"Newbie@Example.com", u"secret", u"", true, false,
                          [&](const gp::ConnectResponse& r) { newProfile = r.profile; });
        pump([&] { n->process(); }, [&] { return newProfile != 0; });
    }
    const std::string newUser = fresh.message("\\newuser\\");
    newOk = newOk && newProfile == 98 &&
            newUser == "\\newuser\\\\email\\newbie@example.com\\nick\\Newbie\\passwordenc\\" +
                           gp::passwordEncode("secret") +
                           "\\productid\\0\\gamename\\juicedpc\\namespaceid\\0\\uniquenick\\\\id\\1" &&
            fresh.message("\\login\\").find("\\userid\\99\\profileid\\98\\response\\") != std::string::npos;
    n->disconnect();
    pump([] {}, [&] { return !fresh.message("\\logout\\").empty(); });
    newOk = newOk && fresh.message("\\logout\\") == "\\logout\\\\sesskey\\4242";
    n.reset();
    fresh.stop();
    std::filesystem::remove(info);
    check(proofOk && newOk, "GameSpy Presence: a proof not the server's (0x0108), a new user, the logout");
}
// The game's user object against the stand-in: the account set, the available check (its name does not
// resolve off GameSpy: available at once), the login, the own record's nick (asked blocking inside the
// login's callback), the local player, the logged-on hook (the friends made), invitable to 10362, the presence
// within a second of a change; a buddy's status, a game invitation and a request into the friends, our answer;
// an error's code into the state; the sign-out.
OJ_INTEGRATION_TEST(Presence, UserObject) {
    namespace gp = oj::net::gp;
    oj::net::startWinsock();
    const std::filesystem::path info = std::filesystem::temp_directory_path() / "OpenJuice.Tests.user.gp.info";
    std::filesystem::remove(info);
    FakeGpcm fake;
    fake.nicks[1234] = "Tester";
    fake.nicks[777] = "Buddy";
    fake.nicks[888] = "Asker";
    bool ok = fake.start();
    oj::net::makeOnlineUser();
    oj::net::OnlineUser* user = oj::net::onlineUser();
    bool hook = false;
    auto pump = [&](float dt, const std::function<bool()>& done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            user->update(dt);
            Sleep(1);
        }
        return done();
    };
    bool loggedOn = false;
    if (ok && user != nullptr) {
        user->server = {"127.0.0.1", fake.port(), info.string()};
        user->loggedOnHook = [&] {
            hook = true;
            oj::net::remakeOnlineFriends();
        };
        user->setCredentials(u"Tester", u"Tester@Example.COM", u"secret", false);
        loggedOn = pump(0.01f, [&] { return user->loggedOn != 0 || user->state != oj::net::UserState::None; }) &&
                   user->loggedOn == 1 && user->state == oj::net::UserState::None && hook &&
                   user->localPlayers.size() == 1 && user->playerNick(user->localPlayers[0]) == u"Tester" &&
                   pump(0.01f, [&] { return !fake.message("\\inviteto\\").empty(); }) &&
                   fake.message("\\inviteto\\") == "\\inviteto\\\\sesskey\\4242\\products\\10362";
    }
    bool presence = false;
    bool friendsOk = false;
    bool answerOk = false;
    bool errorOk = false;
    if (loggedOn) {
        const int32_t p = user->localPlayers[0];
        user->setPlaying(p, 1, 1);
        user->update(1.5f);
        presence = pump(0.01f, [&] { return !fake.message("\\status\\").empty(); }) &&
                   fake.message("\\status\\") ==
                       "\\status\\2\\sesskey\\4242\\statstring\\Playing\\locstring\\juicedpc://0.0.0.0:0/?session=0";
        fake.queue("\\bm\\100\\f\\777\\msg\\|s|2|ss|Playing|ls|juicedpc://1.2.3.4:8667/?session=5|ip|16909060"
                   "|p|0\\final\\");
        oj::net::OnlineFriends* friends = oj::net::onlineFriends();
        // The nick asked not blocking (the fixed build): its answer a process later.
        pump(0.01f, [&] {
            return friends != nullptr && friends->find(777) != -1 &&
                   user->userNick(friends->entries()[static_cast<size_t>(friends->find(777))].user) == u"Buddy";
        });
        const int32_t e = friends != nullptr ? friends->find(777) : -1;
        if (e != -1) {
            const std::optional<oj::net::Address> a = friends->address(e);
            friendsOk = friends->online(e) == 1 && friends->playing(e) == 1 && friends->playingJuiced(e) == 1 &&
                        friends->sameGame(e) == 1 && friends->entries()[static_cast<size_t>(e)].session == 5 &&
                        a && a->ip == 0x04030201u && a->port == 0xDB21 &&
                        user->userNick(friends->entries()[static_cast<size_t>(e)].user) == u"Buddy";
            fake.queue("\\bm\\101\\f\\777\\msg\\|p|10362|l|juicedpc://5.6.7.8:8667/?session=9\\final\\");
            pump(0.01f, [&] { return friends->inviteFrom(e) == 1; });
            friendsOk = friendsOk && friends->inviteFrom(e) == 1 && friends->entries()[static_cast<size_t>(e)].session == 9 &&
                        user->inviteNote(p) == 1;
            fake.queue("\\bm\\2\\f\\888\\msg\\please|signed|0123456789abcdef0123456789abcdef\\final\\");
            pump(0.01f, [&] {
                return friends->find(888) != -1 &&
                       user->userNick(friends->entries()[static_cast<size_t>(friends->find(888))].user) == u"Asker";
            });
            const int32_t r = friends->find(888);
            answerOk = r != -1 && friends->requestFrom(r) == 1 && friends->temporary(r) == 1 &&
                       user->friendNote(p) == 1 &&
                       user->userNick(friends->entries()[static_cast<size_t>(r)].user) == u"Asker";
            if (r != -1) {
                friends->answer(r, false);
                answerOk = answerOk && friends->find(888) == -1;
                user->update(0.01f);
                answerOk = answerOk && pump(0.01f, [&] { return !fake.message("\\authadd\\").empty(); }) &&
                           fake.message("\\authadd\\") ==
                               "\\authadd\\\\sesskey\\4242\\fromprofileid\\888\\sig\\0123456789abcdef0123456789abcdef";
            }
            friends->update();
            // The notes' and the refresh's player not referenced (the fixed build; the stock took a reference at each,
            // never let go).
            answerOk = answerOk && user->friendNote(p) == 0 && user->inviteNote(p) == 0 &&
                       user->players()[static_cast<size_t>(p)].refs == 1;
        }
        // After the login a non-fatal error changes nothing (the fixed build; the stock's state 10 and logged off), a
        // fatal one ends it.
        fake.queue("\\error\\\\err\\1537\\errmsg\\Request refused\\final\\");
        for (int i = 0; i < 200; ++i) {
            user->update(0.01f);
            Sleep(1);
        }
        const bool kept = user->state == oj::net::UserState::None && user->loggedOn == 1;
        fake.queue("\\error\\\\err\\6\\fatal\\\\errmsg\\Forced disconnect\\final\\");
        errorOk = kept && pump(0.01f, [&] { return user->state != oj::net::UserState::None; }) &&
                  user->state == oj::net::UserState::ForcedDisconnect && user->loggedOn == 0;
    }
    oj::net::endOnlineFriends(true);
    oj::net::endOnlineUser();
    const bool signedOut = oj::net::onlineUser() == nullptr && !oj::net::gpConnection();
    fake.stop();
    std::filesystem::remove(info);
    check(ok && loggedOn && presence && friendsOk && answerOk && errorOk && signedOut,
          "online user: login, own nick, hook, presence, buddy status, invitation, request answered, error state");
    // The players met lately (0x004E7D30, 0x004E7C90): the latest first, each referenced, a profile there before
    // moved to the front, ten at most (the oldest let go), taken off by profile.
    {
        oj::net::makeOnlineUser();
        oj::net::OnlineUser* u = oj::net::onlineUser();
        std::vector<int32_t> users;
        for (int32_t i = 0; i < 12; ++i) users.push_back(u->addUser(100 + i, u"P"));
        oj::net::RecentPlayers& recent = oj::net::recentPlayers();
        recent.count = 0;
        for (int32_t i = 0; i < 11; ++i) oj::net::recentAdd(users[static_cast<size_t>(i)]);
        const bool capped = recent.count == 10 && recent.users[0] == users[10] && recent.users[9] == users[1] &&
                            u->record(users[0]).refs == 1 && u->record(users[10]).refs == 2;
        const int32_t same = u->addUser(105, u"P");
        oj::net::recentAdd(same);
        const bool moved = recent.count == 10 && recent.users[0] == same && u->record(users[5]).refs == 1;
        oj::net::recentRemove(users[10]);
        const bool removed = recent.count == 9 && recent.users[0] == same && recent.users[8] == users[1] &&
                             recent.users[9] == -1 && u->record(users[10]).refs == 1;
        recent.count = 0;
        oj::net::endOnlineUser();
        check(capped && moved && removed, "online: the recent players, latest first, ten at most, by profile");
    }
}
// The fixed build: the friends' refresh goes on after a request gone (both requests of ten refreshes gone at the
// tenth; the stock's refresh ended there, the second gone a refresh later).
OJ_INTEGRATION_TEST(Presence, RefreshAfterARequestGone) {
    oj::net::makeOnlineUser();
    oj::net::OnlineUser* u = oj::net::onlineUser();
    oj::net::OnlineFriends* f = oj::net::referenceFriends();
    const int32_t first = u->addUser(501, u"First");
    const int32_t second = u->addUser(502, u"Second");
    f->addFriend(first, 0);
    f->addFriend(second, 0);
    const bool listed = f->list().size() == 2;
    for (int i = 0; i < 10; ++i) f->update();
    const bool gone = f->list().empty();
    f->release();
    oj::net::endOnlineFriends(true);
    oj::net::endOnlineUser();
    check(listed && gone, "online friends: both requests of ten refreshes gone at the tenth");
}

// The fixed build: a user object made while an earlier one lives (held by its friends) lets the earlier connection
// go, and the earlier object's sign-out (its friends let go as the new login makes them again) leaves the new one's
// connection (the stock's destroyed it).
OJ_INTEGRATION_TEST(Presence, EarlierUserLeavesTheNewConnection) {
    namespace gp = oj::net::gp;
    oj::net::startWinsock();
    const std::filesystem::path info = std::filesystem::temp_directory_path() / "OpenJuice.Tests.users.gp.info";
    std::filesystem::remove(info);
    FakeGpcm firstServer;
    FakeGpcm secondServer;
    firstServer.nicks[1234] = "Tester";
    secondServer.nicks[1234] = "Tester";
    const bool ok = firstServer.start() && secondServer.start();
    auto login = [&](FakeGpcm& server) {
        oj::net::makeOnlineUser();
        oj::net::OnlineUser* user = oj::net::onlineUser();
        user->server = {"127.0.0.1", server.port(), info.string()};
        user->loggedOnHook = [] { oj::net::remakeOnlineFriends(); };
        user->setCredentials(u"Tester", u"tester@example.com", u"secret", false);
        for (int i = 0; i < 3000 && user->loggedOn == 0 && user->state == oj::net::UserState::None; ++i) {
            user->update(0.01f);
            Sleep(1);
        }
        return user->loggedOn == 1;
    };
    const bool firstIn = ok && login(firstServer);
    // The game's reference let go; its friends still hold the object.
    if (oj::net::OnlineUser* earlier = oj::net::onlineUser(); earlier != nullptr) oj::net::endOnlineUser();
    const bool secondIn = ok && login(secondServer);
    const std::shared_ptr<gp::Connection> c = oj::net::gpConnection();
    const bool kept = c != nullptr && c->state() == gp::State::LoggedIn;
    oj::net::endOnlineFriends(true);
    oj::net::endOnlineUser();
    firstServer.stop();
    secondServer.stop();
    std::filesystem::remove(info);
    check(firstIn && secondIn && kept, "online user: an earlier user object's sign-out leaves the new connection");
}

// GameSpy Presence's search manager (0x00571A80; nothing of the game starts a search): a profile search's two
// pages (the second asked with \skip\ by itself) and the profiles that have us as a buddy, from a stand-in of
// gpsp.gamespy.com answering each connection once.
OJ_INTEGRATION_TEST(Presence, SearchManager) {
    namespace gp = oj::net::gp;
    oj::net::startWinsock();
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::availableGameName() = "juicedpc";
    oj::net::TcpListener searchServer;
    std::vector<std::string> asked;
    std::mutex askedMutex;
    std::atomic<bool> stopSearch{false};
    const bool listening = searchServer.listen(0);
    std::thread searchThread([&] {
        while (!stopSearch) {
            oj::net::TcpConnection c;
            if (!searchServer.accept(c)) {
                Sleep(1);
                continue;
            }
            std::string in;
            for (int i = 0; i < 2000 && !stopSearch; ++i) {
                c.poll();
                in.append(c.received().begin(), c.received().end());
                c.received().clear();
                if (in.find("\\final\\") != std::string::npos) break;
                Sleep(1);
            }
            {
                std::lock_guard<std::mutex> lock(askedMutex);
                asked.push_back(in);
            }
            if (in.rfind("\\search\\", 0) == 0 && in.find("\\skip\\") == std::string::npos) {
                c.send(std::string("\\bsr\\5\\nick\\Abc\\uniquenick\\Unique\\email\\a@b.c\\x\\y\\bsr\\6\\nick\\Def"
                                   "\\bsrdone\\\\more\\1\\final\\"));
            } else if (in.rfind("\\search\\", 0) == 0) {
                c.send(std::string("\\bsr\\7\\nick\\Ghi\\bsrdone\\\\more\\0\\final\\"));
            } else if (in.rfind("\\others\\", 0) == 0) {
                c.send(std::string("\\others\\\\o\\9\\nick\\Jkl\\first\\Jay\\last\\Kay\\odone\\\\final\\"));
            }
            for (int i = 0; i < 200; ++i) {
                c.poll();
                Sleep(1);
            }
        }
    });
    gp::Connection::Server server;
    server.host = "127.0.0.1";
    server.port = 1;
    server.infoFile = (std::filesystem::temp_directory_path() / "OpenJuice.Tests.gpsearch.info").string();
    server.searchHost = "127.0.0.1";
    server.searchPort = searchServer.port();
    std::shared_ptr<gp::Connection> c;
    const bool searchMade = listening && gp::Connection::create(c, 0, 0, server) == gp::Result::NoError;
    std::vector<gp::ProfileSearchResponse> pages;
    gp::ReverseBuddiesResponse others;
    bool othersCame = false;
    if (searchMade) {
        c->profileSearch("Abc", "", "A@B.C", "", "", 0, 0, false,
                         [&](gp::ProfileSearchResponse& r) { pages.push_back(r); });
        gp::Search s;
        s.type = 7;
        s.onOthers = [&](const gp::ReverseBuddiesResponse& r) {
            others = r;
            othersCame = true;
        };
        c->startSearch(s, false);
        for (int i = 0; i < 4000 && !(pages.size() == 2 && othersCame && c->searchCount() == 0); ++i) {
            c->process();
            Sleep(1);
        }
    }
    stopSearch = true;
    searchThread.join();
    searchServer.close();
    std::string first, second;
    {
        std::lock_guard<std::mutex> lock(askedMutex);
        for (const std::string& m : asked) {
            if (m.rfind("\\search\\", 0) == 0) (first.empty() ? first : second) = m;
        }
    }
    const bool pagesOk = pages.size() == 2 && pages[0].numMatches == 2 && pages[0].more == gp::kMore &&
                         pages[0].matches[0].profile == 5 && pages[0].matches[0].nick == u"Abc" &&
                         pages[0].matches[0].uniquenick == u"Unique" && pages[0].matches[0].email == u"a@b.c" &&
                         pages[0].matches[1].profile == 6 && pages[1].numMatches == 1 &&
                         pages[1].more == gp::kDone && pages[1].matches[0].nick == u"Ghi";
    const bool askedOk = first == "\\search\\\\sesskey\\0\\profileid\\0\\namespaceid\\0\\nick\\Abc\\email\\a@b.c"
                                  "\\gamename\\juicedpc\\final\\" &&
                         second.find("\\skip\\2\\gamename\\") != std::string::npos;
    const bool othersOk = othersCame && others.numProfiles == 1 && others.profiles[0].profile == 9 &&
                          others.profiles[0].nick == u"Jkl" && others.profiles[0].firstname == u"Jay" &&
                          others.profiles[0].lastname == u"Kay";
    check(searchMade && pagesOk && askedOk && othersOk && c->searchCount() == 0,
          "gp: the search manager's profile search (two pages, the second by itself) and reverse buddies");
}
// GameSpy Presence's peers and transfers (none of it reached by the game: it logs in behind the firewall, its
// buddies have no port, it sets no transfer callback). Without the firewall a listening socket: a peer that
// comes authorized by its signature (MD5 of our password, our profile and its own), its buddy message, its
// ping answered through it, its request of files answered "not accepting" without the callback, told to the
// callback with it and then cancelled; a transfer of ours sent to it after its pong (the request, the reply,
// the file's begin, bytes and end with its MD5). A peer of ours to a buddy with a port: its signature asked of
// the server, our \auth\ and its \aack\, the ping's answer queued on it until then.
OJ_INTEGRATION_TEST(Presence, PeersAndTransfers) {
    namespace gp = oj::net::gp;
    oj::net::startWinsock();
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::availableGameName() = "juicedpc";
    const std::filesystem::path info = std::filesystem::temp_directory_path() / "OpenJuice.Tests.gppeer.info";
    const std::filesystem::path sent = std::filesystem::temp_directory_path() / "OpenJuice.Tests.gppeer.txt";
    std::filesystem::remove(info);
    {
        std::ofstream f(sent, std::ios::binary);
        f << "12345";
    }
    auto message = [](int type, const std::string& header, const std::string& text) {
        std::string m = std::format("\\m\\{}{}\\len\\{}\\msg\\\n", type, header, text.size());
        m += text;
        m.push_back('\0');
        return m;
    };
    FakeGpcm fake;
    fake.nicks[1234] = "Tester";
    fake.nicks[777] = "Buddy";
    bool ok = fake.start();
    std::shared_ptr<gp::Connection> c;
    ok = ok && gp::Connection::create(c, 0, 0, {"127.0.0.1", fake.port(), info.string()}) == gp::Result::NoError;
    oj::net::TcpConnection them;
    oj::net::TcpConnection buddy;
    oj::net::TcpListener buddyListener;
    std::string theirs;
    std::string buddys;
    bool buddyIn = false;
    std::vector<gp::BuddyMessageArg> said;
    std::vector<gp::TransferArg> transferEvents;
    auto pump = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            c->process();
            them.poll();
            theirs.append(them.received().begin(), them.received().end());
            them.received().clear();
            if (!buddyIn) buddyIn = buddyListener.accept(buddy);
            if (buddyIn) {
                buddy.poll();
                buddys.append(buddy.received().begin(), buddy.received().end());
                buddy.received().clear();
            }
            Sleep(1);
        }
        return done();
    };
    auto has = [](const std::string& in, const std::string& what) { return in.find(what) != std::string::npos; };
    auto event = [&](int32_t type) -> const gp::TransferArg* {
        for (const gp::TransferArg& e : transferEvents) {
            if (e.type == type) return &e;
        }
        return nullptr;
    };
    bool loggedIn = false;
    if (ok) {
        c->onBuddyMessage = [&](const gp::BuddyMessageArg& a) { said.push_back(a); };
        c->connect(u"Tester", u"tester@example.com", u"secret", false, false, [&](const gp::ConnectResponse& r) {
            loggedIn = r.result == gp::Result::NoError;
        });
        pump([&] { return loggedIn; });
    }
    const uint16_t raw = c ? c->peerPort() : 0;
    const auto peerPort = static_cast<uint16_t>((raw >> 8) | (raw << 8));
    const bool portSent = peerPort != 0 && !has(fake.message("\\login\\"), "\\port\\0\\");
    bool authOk = false;
    if (loggedIn && peerPort != 0) {
        them.connect(oj::net::resolveHost("127.0.0.1"), peerPort);
        them.send("\\auth\\\\pid\\555\\nick\\Peer\\sig\\" + gp::md5Hex("secret1234555") + "\\final\\");
        authOk = pump([&] { return has(theirs, "\\aack\\\\final\\"); });
    }
    bool talkOk = false;
    bool requestOk = false;
    bool sendOk = false;
    if (authOk) {
        them.send(message(1, "", "hello"));
        them.send(message(102, "", "1"));
        them.send(message(200, "\\xfer\\555 1 2\\version\\1\\num\\1\\name0\\a.txt\\size0\\5\\mtime0\\7", ""));
        talkOk = pump([&] {
            return said.size() == 1 && has(theirs, message(103, "", "1")) &&
                   has(theirs, message(201, "\\xfer\\555 1 2\\version\\1\\result\\2", ""));
        });
        talkOk = talkOk && said[0].profile == 555 && said[0].message == u"hello";
        c->onTransfer = [&](const gp::TransferArg& a) { transferEvents.push_back(a); };
        them.send(message(200, "\\xfer\\555 1 3\\version\\1\\num\\1\\name0\\a.txt\\size0\\5\\mtime0\\7", "take"));
        pump([&] { return event(gp::kTransferSendRequest) != nullptr; });
        them.send(message(207, "\\xfer\\555 1 3", ""));
        pump([&] { return event(gp::kTransferCancelled) != nullptr; });
        const gp::TransferArg* asked = event(gp::kTransferSendRequest);
        requestOk = asked != nullptr && asked->num == 1 && asked->message == u"take" &&
                    event(gp::kTransferCancelled) != nullptr &&
                    event(gp::kTransferCancelled)->transfer == asked->transfer;
        // Ours: waiting for the pong, then the request; accepted, the file goes.
        const int32_t ours = c->addSendingTransfer(555, "hello", {{"dir\\a.txt", sent.string()}});
        them.send(message(103, "", "1"));
        pump([&] { return has(theirs, "\\m\\200\\xfer\\0 0 0"); });
        them.send(message(201, "\\xfer\\0 0 0\\version\\1\\result\\0", "ok"));
        sendOk = pump([&] { return event(gp::kTransferDone) != nullptr; });
        const gp::TransferArg* accepted = event(gp::kTransferAccepted);
        const gp::TransferArg* progress = event(gp::kFileProgress);
        sendOk = sendOk &&
                 has(theirs, message(200, "\\xfer\\0 0 0\\version\\1\\num\\1\\name0\\dir/a.txt\\size0\\-1\\mtime0\\0",
                                     "hello")) &&
                 has(theirs, "\\m\\202\\xfer\\0 0 0\\file\\0\\size\\5\\mtime\\") &&
                 has(theirs, message(204, "\\xfer\\0 0 0\\file\\0", "12345")) &&
                 has(theirs, message(203, "\\xfer\\0 0 0\\file\\0\\md5\\827ccb0eea8a706c4c34a16891f84e7b", "")) &&
                 accepted != nullptr && accepted->transfer == ours && accepted->message == u"ok" &&
                 event(gp::kFileBegin) != nullptr && progress != nullptr && progress->num == 5 &&
                 event(gp::kFileEnd) != nullptr && event(gp::kTransferDone)->transfer == ours;
    }
    bool outgoingOk = false;
    if (sendOk && buddyListener.listen(0)) {
        fake.queue(std::format("\\bm\\100\\f\\777\\msg\\|s|1|ss||ls||ip|2130706433|p|{}\\final\\",
                               buddyListener.port()));
        fake.queue("\\bm\\102\\f\\777\\msg\\1\\final\\");
        const std::string auth = "\\auth\\\\pid\\1234\\nick\\Tester\\sig\\0123456789abcdef0123456789abcdef\\final\\";
        pump([&] { return has(buddys, auth); });
        const bool sigLetGo = c->findProfile(777) != nullptr && !c->findProfile(777)->peerSig;
        buddy.send(std::string("\\aack\\\\final\\"));
        outgoingOk = pump([&] { return has(buddys, message(103, "", "1")); }) && sigLetGo &&
                     has(fake.message("\\getprofile\\"), "\\profileid\\777");
    }
    them.close();
    buddy.close();
    buddyListener.close();
    c.reset();
    fake.stop();
    std::filesystem::remove(info);
    std::filesystem::remove(sent);
    check(ok && loggedIn && portSent && authOk && talkOk && requestOk && sendOk && outgoingOk,
          "gp: the peers (an incoming one authorized, its message, ping and requests; one of ours to a buddy) and a "
          "transfer of ours sent over one");
}
