#include "Core/Base/GameFiles.h"
#include "Core/Data/GameData.h"
#include "Mods/Creative.h"
#include "Mods/Packages.h"
#include "Mods/Runtime.h"
#include <algorithm>
#include <filesystem>
#include <jni.h>
#include <string>
namespace {
std::string string(JNIEnv *env, jstring value) {
    const char *chars = env->GetStringUTFChars(value, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}
} // namespace
extern "C" JNIEXPORT jstring JNICALL Java_org_openjuice_android_NativeBridge_validate(JNIEnv *env, jclass,
                                                                                      jstring gamePath,
                                                                                      jstring modPath) {
    std::filesystem::path overlay;
    std::string result;
    try {
        auto game = string(env, gamePath), mods = string(env, modPath);
        oj::mods::runtime::load({});
        oj::mods::packages::discover({});
        if (!mods.empty()) {
            oj::mods::packages::discover(mods);
            overlay = oj::mods::packages::assemble(game);
            oj::mods::runtime::load(mods);
            const auto loaded = oj::mods::runtime::loaded();
            for (const auto &p : oj::mods::packages::list())
                if (p.active && !p.assets &&
                    std::find(loaded.begin(), loaded.end(), p.id + "/main.lua") == loaded.end())
                    throw std::runtime_error("Lua validation failed: " + p.id);
        }
        oj::GameFiles::setOverrideRoot(overlay);
        oj::CrtRandom random;
        oj::GameData data(game);
        std::string error;
        if (!data.load(random, &error))
            throw std::runtime_error(error);
        result =
            "OK: " + std::to_string(data.defs.cars.size()) + " cars loaded. Data and active mods validated.";
    } catch (const std::exception &e) {
        result = std::string("ERROR: ") + e.what();
    }
    oj::GameFiles::setOverrideRoot({});
    if (!overlay.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(overlay, ec);
    }
    return env->NewStringUTF(result.c_str());
}
