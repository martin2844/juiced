#import <AppKit/AppKit.h>
#include "Mods/Creative.h"
#include <algorithm>
#include <chrono>
#include <fstream>

namespace oj::mods {
std::string importImageFile(const std::filesystem::path& path,bool portrait) {
    @autoreleasepool {
        std::error_code error;
        if(std::filesystem::file_size(path,error)>50*1024*1024 || error) return "Choose an image smaller than 50 MB.";
        NSString* file=[NSString stringWithUTF8String:path.string().c_str()];
        NSImage* image=[[NSImage alloc] initWithContentsOfFile:file];
        if(!image || image.size.width<=0 || image.size.height<=0) return "Could not read that image. Try PNG or JPEG.";
        constexpr int n=512;
        NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:n pixelsHigh:n
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
            bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:n*4 bitsPerPixel:32];
        if(!bitmap) return "Could not allocate the image buffer.";
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
        [[NSColor clearColor] set];NSRectFill(NSMakeRect(0,0,n,n));
        const double scale=portrait?std::max(n/image.size.width,n/image.size.height):std::min(n/image.size.width,n/image.size.height);
        const double w=image.size.width*scale,h=image.size.height*scale;
        [image drawInRect:NSMakeRect((n-w)/2,(n-h)/2,w,h) fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1];
        [NSGraphicsContext restoreGraphicsState];
        const auto directory=imageDirectory();std::filesystem::create_directories(directory,error);
        if(error) return "Could not create the profile image folder.";
        const auto name="mod_image_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto destination=directory/(name+".ddt");std::ofstream out(destination,std::ios::binary);
        const uint32_t header[11]={21,n,n,1,2,2,0,3,3,0,1};out.write(reinterpret_cast<const char*>(header),sizeof(header));
        for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
            const auto* p=bitmap.bitmapData+y*bitmap.bytesPerRow+x*4;
            const uint8_t bgra[]={p[2],p[1],p[0],p[3]};out.write(reinterpret_cast<const char*>(bgra),4);
        }
        out.close();if(!out || !imageImported(portrait,name)) {std::filesystem::remove(destination,error);return "Could not save image settings.";}
        return {};
    }
}
std::string importImage(bool portrait) {
    @autoreleasepool {
        NSOpenPanel* panel=[NSOpenPanel openPanel];panel.canChooseDirectories=NO;panel.allowsMultipleSelection=NO;
        panel.allowedFileTypes=@[@"png",@"jpg",@"jpeg",@"bmp"];
        if([panel runModal]!=NSModalResponseOK) return "No image selected.";
        return importImageFile(std::filesystem::path(panel.URL.fileSystemRepresentation),portrait);
    }
}
}
