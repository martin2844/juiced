#include "Core/Career/Money.h"

#include <algorithm>
#include <format>

namespace oj {

std::string groupThousands(int value, int language) {
    const bool negative = value < 0;
    // Stock problem: the most negative int stays negative after the sign is taken off.
    const unsigned magnitude = negative ? 0u - static_cast<unsigned>(value) : static_cast<unsigned>(value);
    const int shown = static_cast<int>(magnitude);
    std::string digits = std::format("{}", shown);
    std::reverse(digits.begin(), digits.end());
    const char separator = language == 3 || language == 1 ? '.' : ',';
    std::string grouped;
    int run = 0;
    for (char c : digits) {
        grouped.push_back(c);
        if (run == 2 && shown > 0) {
            grouped.push_back(separator);
            run = -1;
        }
        ++run;
    }
    if (!grouped.empty() && grouped.back() == separator) grouped.pop_back();
    std::reverse(grouped.begin(), grouped.end());
    return negative ? "-" + grouped : grouped;
}

std::string moneyText(int value, int language) {
    const std::string amount = groupThousands(value, language);
    return language == 1 ? amount + " $" : "$" + amount;
}

}  // namespace oj
