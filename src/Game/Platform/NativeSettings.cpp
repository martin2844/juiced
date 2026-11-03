#if defined(OJ_LINUX) || defined(OJ_ANDROID) || defined(OJ_MACOS)
#include "Game/Platform/Settings.h"
#include "Game/Platform/NativeSettings.h"
#include "Game/Platform/CommandLine.h"
#if defined(OJ_LINUX)
#include "Linux/Paths.h"
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

namespace oj {
namespace {
std::mutex settingsMutex;
std::filesystem::path settingsFile() {
    if (const char* p=std::getenv("JUICED_SUPPORT_DIR"); p && *p)
        return std::filesystem::path(p)/"platform-settings.ini";
#if defined(OJ_LINUX)
    return linux_port::configPath()/"platform-settings.ini";
#else
    if (const char* p=std::getenv("HOME"); p && *p)
        return std::filesystem::path(p)/"Library/Application Support/Juiced/platform-settings.ini";
    return {};
#endif
}
std::map<std::string,uint32_t> values(const std::filesystem::path& path) {
    std::map<std::string,uint32_t> result;
    std::ifstream in(path);std::string name;uint64_t value;
    while(in>>name>>value) if(value<=UINT32_MAX) result[name]=static_cast<uint32_t>(value);
    return result;
}
}
std::optional<uint32_t> readSetting(const char* name) {
    const std::lock_guard lock(settingsMutex);
    const auto all=values(settingsFile());const auto it=all.find(name);
    return it==all.end()?std::nullopt:std::optional<uint32_t>(it->second);
}
bool writeSetting(const char* name,uint32_t value) {
    const std::lock_guard lock(settingsMutex);
    const auto path=settingsFile();if(path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(),error);if(error) return false;
    auto all=values(path);all[name]=value;auto temp=path;temp+=".tmp";
    std::ofstream out(temp,std::ios::trunc);for(const auto& [key,v]:all) out<<key<<' '<<v<<'\n';
    out.close();if(!out) return false;
    std::filesystem::rename(temp,path,error);return !error;
}
void readNativeSettings(Settings& s) {
    struct Field { const char* name;uint32_t Settings::*member;uint32_t maximum; };
    const Field fields[]={
        {"MSAA",&Settings::msaa,16},{"SMAA",&Settings::smaa,1},
        {"VSync",&Settings::vsync,1},{"MaxFPS",&Settings::maxFps,1000},
        {"TextureFilter",&Settings::textureFilter,5},{"LodBias",&Settings::lodBias,4},
        {"ShowFps",&Settings::showFps,1},{"CarPaint",&Settings::carPaint,1},
        {"WorldLight",&Settings::worldLight,2},{"Sky",&Settings::sky,2},
        {"Headlights",&Settings::headlights,2},{"HeatHaze",&Settings::heatHaze,1},
        {"HeatHazeStrength",&Settings::heatHazeStrength,50},
        {"Reflections",&Settings::reflections,1},{"LightFlares",&Settings::lightFlares,1},
        {"Shadows",&Settings::shadows,2},{"AmbientOcclusion",&Settings::ambientOcclusion,1},
        {"audioflags",&Settings::audioFlags,63}};
    for(const auto& f:fields) if(const auto v=readSetting(f.name);v && *v<=f.maximum) s.*(f.member)=*v;
}
void applyNativeGraphicsOptions(Settings& s,const LaunchOptions& o) {
    if(o.worldLight && *o.worldLight<=2) s.worldLight=*o.worldLight;
    if(o.sky && *o.sky<=2) s.sky=*o.sky;
    if(o.headlights && *o.headlights<=2) s.headlights=*o.headlights;
    if(o.carPaint && *o.carPaint<=1) s.carPaint=*o.carPaint;
}
}
#endif
