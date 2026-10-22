#include "Mods/Creative.h"
#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#include <chrono>
#include <fstream>
#include <vector>
namespace oj::mods {
std::string importImageFile(const std::filesystem::path &path, bool portrait) {
    @autoreleasepool {
        std::error_code ec;
        if (std::filesystem::file_size(path, ec) > 50 * 1024 * 1024 || ec)
            return "Choose an image smaller than 50 MB.";
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, nullptr);
        if (!source)
            return "That file is not a supported image.";
        NSDictionary *options = @{
            (id)kCGImageSourceCreateThumbnailFromImageAlways : @YES,
            (id)kCGImageSourceCreateThumbnailWithTransform : @YES,
            (id)kCGImageSourceThumbnailMaxPixelSize : @512
        };
        CGImageRef image = CGImageSourceCreateThumbnailAtIndex(source, 0, (__bridge CFDictionaryRef)options);
        CFRelease(source);
        if (!image)
            return "Could not read that image. Try PNG or JPEG.";
        constexpr int n = 512;
        std::vector<uint8_t> pixels(n * n * 4);
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef context = CGBitmapContextCreate(pixels.data(), n, n, 8, n * 4, space,
                                                     kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!context) {
            CGImageRelease(image);
            return "Could not allocate the image buffer.";
        }
        double w = CGImageGetWidth(image), h = CGImageGetHeight(image);
        double scale = portrait ? std::max(n / w, n / h) : std::min(n / w, n / h);
        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
        CGContextDrawImage(context, CGRectMake((n - w * scale) / 2, (n - h * scale) / 2, w * scale, h * scale), image);
        CGContextRelease(context);
        CGImageRelease(image);
        // DDT uses straight-alpha BGRA; CoreGraphics supplied premultiplied RGBA.
        for (size_t i = 0; i < pixels.size(); i += 4) {
            auto a = pixels[i + 3];
            uint8_t r = pixels[i], g = pixels[i + 1], b = pixels[i + 2];
            auto straight = [&](uint8_t v) { return uint8_t(a ? std::min(255, int(v) * 255 / int(a)) : 0); };
            pixels[i] = straight(b);
            pixels[i + 1] = straight(g);
            pixels[i + 2] = straight(r);
        }
        auto directory = imageDirectory();
        std::filesystem::create_directories(directory, ec);
        if (ec)
            return "Could not create the profile image folder.";
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::string name = "mod_image_" + std::to_string(stamp);
        std::ofstream out(directory / (name + ".ddt"), std::ios::binary);
        uint32_t header[11] = {21, n, n, 1, 2, 2, 0, 3, 3, 0, 1};
        out.write(reinterpret_cast<char *>(header), sizeof(header));
        out.write(reinterpret_cast<char *>(pixels.data()), pixels.size());
        out.close();
        if (!out)
            return "Could not save the imported image.";
        if (!imageImported(portrait, name))
            return "Could not save this profile’s image settings.";
        return {};
    }
}
std::string importImage(bool portrait) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.title = portrait ? @"Choose your driver photo" : @"Choose your crew logo";
        panel.message = portrait ? @"Your photo is cropped to a square and saved with this profile."
                                 : @"Transparent PNG works best. The complete image will fit inside the decal.";
        if ([panel runModal] != NSModalResponseOK)
            return "No image selected.";
        return importImageFile(panel.URL.fileSystemRepresentation, portrait);
    }
}
} // namespace oj::mods
