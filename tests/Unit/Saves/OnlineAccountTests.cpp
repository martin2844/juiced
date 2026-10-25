// Unit tests: the fixed build's protected GameSpy account (Core/Saves/OnlineAccount).

#include "Framework/Test.h"

#include "Core/Saves/OnlineAccount.h"
#include "Core/Saves/SaveEntries.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

using oj::save::OnlineAccount;
using oj::test::check;

}  // namespace

#ifdef _WIN32
// Protected and read back: the three texts as they were; the entry holds none of them in plain text.
OJ_UNIT_TEST(OnlineAccount, ProtectedAndReadBack) {
    const OnlineAccount account{L"Racer_01", L"racer@example.com", L"a longer password 1234"};
    const std::vector<uint8_t> entry = oj::save::protectAccount(account);
    const std::optional<OnlineAccount> back = oj::save::unprotectAccount(entry);
    const std::u16string password = u"a longer password 1234";
    const auto* bytes = reinterpret_cast<const uint8_t*>(password.data());
    const bool hidden = std::search(entry.begin(), entry.end(), bytes, bytes + password.size() * 2) == entry.end();
    check(!entry.empty() && back && back->nick == account.nick && back->email == account.email &&
              back->password == account.password && hidden,
          "online account: protected, read back whole, the password not in the entry");
}

// An entry that does not decrypt (changed, or none) gives no account.
OJ_UNIT_TEST(OnlineAccount, DamagedEntryIsNone) {
    std::vector<uint8_t> entry = oj::save::protectAccount({L"Nick", L"n@example.com", L"secret"});
    if (!entry.empty()) entry[entry.size() / 2] ^= 0x5A;
    check(!entry.empty() && !oj::save::unprotectAccount(entry) && !oj::save::unprotectAccount({}),
          "online account: a changed entry and an empty one give none");
}

#else
OJ_UNIT_TEST(OnlineAccount, NoPlaintextFallback) {
    check(oj::save::protectAccount({L"Nick", L"n@example.com", L"secret"}).empty() &&
          !oj::save::unprotectAccount({1, 2, 3}),
          "platform without DPAPI does not persist credentials or accept foreign blobs");
}
#endif

// The settings' fields: taken and put back (31, 51 and 31 characters with their 0).
OJ_UNIT_TEST(OnlineAccount, SettingsFields) {
    oj::save::Settings settings = oj::save::Settings::defaults(0);
    const OnlineAccount account{L"Nick", L"nick@example.com", L"secret"};
    account.into(settings);
    const OnlineAccount taken = OnlineAccount::of(settings);
    OnlineAccount{}.into(settings);
    check(taken.nick == L"Nick" && taken.email == L"nick@example.com" && taken.password == L"secret" &&
              OnlineAccount::of(settings).empty(),
          "online account: the settings' fields taken, then cleared");
}
