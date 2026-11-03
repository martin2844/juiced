#pragma once
namespace oj {
struct Settings;
struct LaunchOptions;
void readNativeSettings(Settings& settings);
void applyNativeGraphicsOptions(Settings& settings,const LaunchOptions& options);
}
