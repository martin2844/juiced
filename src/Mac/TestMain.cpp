#include "Core/Base/Text.h"
#include <string>
#include <vector>
int wmain(int, wchar_t **);
int main(int argc, char **argv) {
    std::vector<std::wstring> strings;
    for (int i = 0; i < argc; i++)
        strings.push_back(oj::toWide(argv[i]));
    std::vector<wchar_t *> args;
    for (auto &s : strings)
        args.push_back(s.data());
    return wmain(argc, args.data());
}
