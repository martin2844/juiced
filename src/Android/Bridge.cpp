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

#include "Android/SaveEditor.h"
namespace {
struct EditorGame {
    std::filesystem::path overlay;
    std::filesystem::path saveRoot;
    std::unique_ptr<oj::GameData> data;
    ~EditorGame() {
        oj::GameFiles::setOverrideRoot({});
        oj::mods::runtime::load({});
        oj::mods::packages::discover({});
        if (!overlay.empty()) { std::error_code ec; std::filesystem::remove_all(overlay, ec); }
    }
    void load(const std::filesystem::path& files, bool modded) {
        oj::GameFiles::setOverrideRoot({});
        oj::mods::runtime::load({});
        oj::mods::packages::discover({});
        saveRoot = files / "saves";
        if (modded) {
            oj::mods::packages::discover(files / "mods");
            if (!oj::mods::packages::available()) throw std::runtime_error("No enabled mod setup is available");
            overlay = oj::mods::packages::assemble(files / "game");
            oj::GameFiles::setOverrideRoot(overlay);
            oj::mods::runtime::load(files / "mods");
            saveRoot = oj::mods::packages::saveDirectory(files / "ModSaves");
        }
        data = std::make_unique<oj::GameData>(files / "game");
        oj::CrtRandom random;
        std::string error;
        if (!data->load(random, &error)) throw std::runtime_error(error);
    }
};
std::vector<std::byte> saveBytes(JNIEnv* env, jbyteArray input) {
    const auto n = env->GetArrayLength(input);
    if (n > 2 * 1024 * 1024) throw std::runtime_error("Save is too large");
    std::vector<std::byte> bytes(n);
    env->GetByteArrayRegion(input, 0, n, reinterpret_cast<jbyte*>(bytes.data()));
    return bytes;
}
void editorError(JNIEnv* env, const std::exception& e) {
    env->ThrowNew(env->FindClass("java/io/IOException"), e.what());
}
}
extern "C" JNIEXPORT jstring JNICALL Java_org_openjuice_android_NativeBridge_saveCatalog(
    JNIEnv* env, jclass, jstring files, jboolean modded) {
    try {
        EditorGame game; game.load(string(env, files), modded);
        std::string out = "{\"root\":" + oj::saveedit::quote(game.saveRoot.string()) + ",\"cars\":[";
        bool first = true;
        for (size_t i = 0; i < game.data->defs.cars.size(); ++i) {
            const auto& car = game.data->defs.cars[i];
            if (car.id == 0) continue; // NONE is an engine placeholder, not a vehicle.
            if (!first) out += ',';
            first = false;
            out += "{\"model\":" + oj::saveedit::quote(car.model) + ",\"name\":" + oj::saveedit::quote(car.displayName) + "}";
        }
        return env->NewStringUTF((out + "]}").c_str());
    } catch (const std::exception& e) { editorError(env, e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_openjuice_android_NativeBridge_inspectSave(
    JNIEnv* env, jclass, jstring files, jboolean modded, jbyteArray bytes) {
    try {
        EditorGame game; game.load(string(env, files), modded);
        const auto result = oj::saveedit::describe(*game.data, saveBytes(env, bytes));
        return env->NewStringUTF(result.c_str());
    } catch (const std::exception& e) { editorError(env, e); return nullptr; }
}
extern "C" JNIEXPORT jbyteArray JNICALL Java_org_openjuice_android_NativeBridge_editSave(
    JNIEnv* env, jclass, jstring files, jboolean modded, jbyteArray bytes, jint money,
    jintArray respect, jobjectArray cars) {
    try {
        if (env->GetArrayLength(respect) != 8 || env->GetArrayLength(cars) > 55)
            throw std::runtime_error("Invalid save changes");
        oj::saveedit::Changes changes;
        changes.money = money;
        env->GetIntArrayRegion(respect, 0, 8, changes.respect.data());
        for (jsize i = 0; i < env->GetArrayLength(cars); ++i) {
            auto model = static_cast<jstring>(env->GetObjectArrayElement(cars, i));
            changes.addCars.push_back(string(env, model));
            env->DeleteLocalRef(model);
        }
        EditorGame game; game.load(string(env, files), modded);
        auto result = oj::saveedit::edit(*game.data, saveBytes(env, bytes), changes);
        auto out = env->NewByteArray(result.size());
        env->SetByteArrayRegion(out, 0, result.size(), reinterpret_cast<const jbyte*>(result.data()));
        return out;
    } catch (const std::exception& e) { editorError(env, e); return nullptr; }
}
