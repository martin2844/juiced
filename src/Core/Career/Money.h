#pragma once
// Amounts of money and respect as the career's windows show them.

#include <string>

namespace oj {

// 0x004A3AF0: a whole number with its thousands marked, "." for French and Italian (languages 1 and 3), else ",";
// a negative number gets a "-" first.
std::string groupThousands(int value, int language);

// 0x004A3A90: money: "<amount> $" in French, else "$<amount>".
std::string moneyText(int value, int language);

}  // namespace oj
