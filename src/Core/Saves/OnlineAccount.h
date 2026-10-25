#pragma once
// The fixed build's GameSpy account of a driver, protected: the stock keeps it in settings.dat in plain text (the
// nick +0xE4, the email +0x122, the password +0x188). The fixed build writes those fields empty and keeps the account
// in an entry of its own in the driver's juicey.dat, encrypted by Windows (DPAPI, CryptProtectData: only the same
// Windows user on the same machine reads it back). The stock game ignores the entry, as the mods' entries.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj::save {

struct Settings;

// The entry's name in juicey.dat.
inline constexpr const char* kOnlineAccountEntry = "OnlineAccount.dat";

struct OnlineAccount {
    std::wstring nick;
    std::wstring email;
    std::wstring password;

    bool empty() const { return nick.empty() && email.empty() && password.empty(); }
    // The settings' fields (+0xE4, +0x122, +0x188: 31, 51 and 31 characters).
    static OnlineAccount of(const Settings& settings);
    void into(Settings& settings) const;
};

// The account encrypted (a version, then each text's length and its UTF-16 characters); empty when Windows refuses.
std::vector<uint8_t> protectAccount(const OnlineAccount& account);
// The account of an entry; none when it does not decrypt (another Windows user or machine) or does not hold.
std::optional<OnlineAccount> unprotectAccount(const std::vector<uint8_t>& entry);

}  // namespace oj::save
