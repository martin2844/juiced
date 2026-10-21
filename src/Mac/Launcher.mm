#import <AppKit/AppKit.h>
#include <filesystem>

std::filesystem::path chooseGameFolder() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.title = @"Choose your Juiced game data";
        panel.message = @"Select the extracted game folder containing tracks and carmodels.dat.";
        panel.canChooseDirectories = YES;
        panel.canChooseFiles = NO;
        panel.allowsMultipleSelection = NO;
        if ([panel runModal] == NSModalResponseOK)
            return std::filesystem::path(panel.URL.fileSystemRepresentation);
        return {};
    }
}
