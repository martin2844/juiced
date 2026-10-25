#include "Mods/Creative.h"
#include <SDL3/SDL.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <vector>

namespace oj::mods {
namespace {
using Image = std::unique_ptr<GdkPixbuf, decltype(&g_object_unref)>;
}
std::string importImageFile(const std::filesystem::path& path, bool portrait) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) > 50 * 1024 * 1024 || ec)
        return "Choose an image smaller than 50 MB.";
    GError* error = nullptr;
    Image decoded(gdk_pixbuf_new_from_file_at_scale(path.c_str(), 512, 512, TRUE, &error), g_object_unref);
    if (error) g_error_free(error);
    if (!decoded) return "Could not read that image. Try PNG or JPEG.";
    Image image(gdk_pixbuf_apply_embedded_orientation(decoded.get()), g_object_unref);
    if (!image) return "Could not orient the image.";
    constexpr int n = 512;
    Image canvas(gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, n, n), g_object_unref);
    if (!canvas) return "Could not allocate the image buffer.";
    gdk_pixbuf_fill(canvas.get(), 0);
    const double w = gdk_pixbuf_get_width(image.get()), h = gdk_pixbuf_get_height(image.get());
    const double scale = portrait ? std::max(n / w, n / h) : std::min(n / w, n / h);
    const double x = (n - w * scale) / 2, y = (n - h * scale) / 2;
    const int left = std::max(0, int(x)), top = std::max(0, int(y));
    const int right = std::min(n, int(x + w * scale)), bottom = std::min(n, int(y + h * scale));
    gdk_pixbuf_scale(image.get(), canvas.get(), left, top, right-left, bottom-top,
                    x, y, scale, scale, GDK_INTERP_BILINEAR);
    std::vector<uint8_t> pixels(n*n*4);
    const auto* rgba = gdk_pixbuf_read_pixels(canvas.get());
    const int pitch = gdk_pixbuf_get_rowstride(canvas.get());
    for (int row=0; row<n; ++row) for(int col=0; col<n; ++col) {
        const auto* source = rgba + row*pitch + col*4;
        auto* dest = pixels.data() + (row*n+col)*4;
        dest[0]=source[2]; dest[1]=source[1]; dest[2]=source[0]; dest[3]=source[3];
    }
    const auto directory = imageDirectory();
    std::filesystem::create_directories(directory, ec);
    if (ec) return "Could not create the profile image folder.";
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string name = "mod_image_" + std::to_string(stamp);
    const auto file = directory / (name + ".ddt");
    std::ofstream out(file, std::ios::binary);
    uint32_t header[11] = {21,n,n,1,2,2,0,3,3,0,1};
    out.write(reinterpret_cast<const char*>(header), sizeof(header));
    out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    out.close();
    if (!out) { std::filesystem::remove(file, ec); return "Could not save the imported image."; }
    if (!imageImported(portrait, name)) { std::filesystem::remove(file, ec); return "Could not save the image settings."; }
    return {};
}
std::string importImage(bool portrait) {
    std::promise<std::string> selected;
    auto result = selected.get_future();
    const SDL_DialogFileFilter filters[] = {{"Images", "png;jpg;jpeg;bmp"}};
    SDL_ShowOpenFileDialog([](void* userdata, const char* const* paths, int) {
        static_cast<std::promise<std::string>*>(userdata)->set_value(paths && paths[0] ? paths[0] : "");
    }, &selected, SDL_GetKeyboardFocus(), filters, 1, nullptr, false);
    // SDL's portal backend dispatches completion through the main-thread event pump.
    while (result.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) SDL_PumpEvents();
    const auto path = result.get();
    return path.empty() ? "No image selected." : importImageFile(path, portrait);
}
} // namespace oj::mods
