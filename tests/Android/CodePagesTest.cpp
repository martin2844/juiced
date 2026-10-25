#include "Android/CodePages.h"
#include <iostream>
#include <stdexcept>
int main() {
    using oj::android::widenPage;
    if (widenPage("caf\xe9", 1252) != L"café" || widenPage("\xc0\xe1", 1251) != L"Аб" ||
        widenPage("\xa5", 1250) != L"Ą" || widenPage("\x81", 1252) != L"\ufffd")
        throw std::runtime_error("Code-page conversion failed");
    std::cout << "PASS: Western, Cyrillic, Central European and invalid text bytes\n";
}
