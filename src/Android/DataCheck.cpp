// Host-side verification of the same engine and POSIX adapters used by Android.
#include "Core/Data/GameData.h"
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    oj::CrtRandom random;
    oj::GameData data(argv[1]);
    std::string error;
    if (!data.load(random, &error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "Loaded " << data.defs.cars.size() << " car definitions with Android core configuration\n";
    return data.defs.cars.empty() ? 1 : 0;
}
