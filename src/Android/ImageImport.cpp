#ifdef __ANDROID__
#include "Mods/Creative.h"
#include <SDL3/SDL.h>
#include <jni.h>
namespace oj::mods {
std::string importImageFile(const std::filesystem::path &, bool) {
    return "Choose an image using the Android picker.";
}
std::string importImage(bool portrait) {
    auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity)
        return "Image picker is unavailable.";
    auto cls = env->GetObjectClass(activity);
    auto method = env->GetMethodID(cls, "importImage", "(Ljava/lang/String;Z)Ljava/lang/String;");
    auto folder = env->NewStringUTF(imageDirectory().string().c_str());
    auto value = static_cast<jstring>(env->CallObjectMethod(activity, method, folder, portrait));
    std::string result = "Image import failed.";
    if (!env->ExceptionCheck() && value) {
        const char *text = env->GetStringUTFChars(value, nullptr);
        std::string name(text);
        env->ReleaseStringUTFChars(value, text);
        if (name.starts_with("mod_image_")) {
            result = imageImported(portrait, name) ? "" : "Could not save image settings.";
        } else
            result = name;
        env->DeleteLocalRef(value);
    } else
        env->ExceptionClear();
    env->DeleteLocalRef(folder);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return result;
}
} // namespace oj::mods
#else
#include "../Linux/ImageImport.cpp"
#endif
