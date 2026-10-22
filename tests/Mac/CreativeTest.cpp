#include "../Framework/Fakes/NullRenderDevice.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Data/GameData.h"
#include "Core/Formats/Ddt.h"
#include "Game/Cars/CarModel.h"
#include "Mods/CreativeMenu.h"
#include "Mods/Runtime.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace oj;
void require(bool value, const char *label) {
    if (!value)
        throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() / ("juiced-creative-" + std::to_string(getpid()));
    try {
        mods::runtime::load(JUICED_TEST_MODS);
        mods::configure(folder);
        mods::profileName("test driver");
        Car car;
        car.setModel("FERRARI_458_STOCK");
        car.neon = 0xff204080;
        require(mods::neonTexture(car).empty(), "unconfigured neon retains original texture");
        mods::saveNeon(car, {0x204080, 1, 2});
        require(mods::neonColour(car, 0) != mods::neonColour(car, .5), "pulse changes brightness over time");
        mods::saveNeon(car, {0x204080, 3, 2});
        require(mods::neonColour(car, 0) != mods::neonColour(car, .5), "RGB cycle changes hue");
        mods::saveNeon(car, {0x204080, 2, 2});
        auto scan = mods::imageBytes("mod_neon_scan_0"), other = mods::imageBytes("mod_neon_scan_8");
        require(scan && other && *scan != *other, "scan moves the glow spatially instead of just pulsing");
        mods::profileName("other driver");
        require(mods::neonTexture(car).empty(), "effects isolated between drivers");
        mods::profileName("test driver");
        require(mods::neon(car).effect == 2, "effect reloads from sidecar");
        // A two-by-two uncompressed BMP, red left and blue right. No external image dependencies.
        std::vector<uint8_t> bmp(62);
        bmp[0] = 'B';
        bmp[1] = 'M';
        auto u32 = [&](size_t p, uint32_t v) { std::memcpy(bmp.data() + p, &v, 4); };
        u32(2, 62);
        u32(10, 54);
        u32(14, 40);
        u32(18, 2);
        u32(22, 1);
        bmp[26] = 1;
        bmp[28] = 24;
        u32(34, 8);
        for (size_t y = 0; y < 1; ++y) {
            bmp[54 + y * 8 + 2] = 255;
            bmp[54 + y * 8 + 3] = 255;
        }
        std::filesystem::create_directories(folder);
        auto source = folder / "sample.bmp";
        std::ofstream image(source, std::ios::binary);
        image.write(reinterpret_cast<char *>(bmp.data()), bmp.size());
        image.close();
        require(mods::importImageFile(source, true).empty(), "actual image decoder imports portrait");
        const auto portrait = mods::imageName(true);
        auto bytes = mods::imageBytes(portrait);
        Ddt texture;
        require(bytes && texture.parse(*bytes) && texture.width == 512 && texture.height == 512,
                "portrait is bounded native512 texture");
        require(mods::uiTexture("ui_crewmember_00") == portrait, "silhouette resolves to uploaded portrait");
        require(mods::importImageFile(source, false).empty(), "logo import succeeds");
        auto logo = mods::imageName(false);
        auto logoBytes = mods::imageBytes(logo);
        Ddt logoTexture;
        require(logoBytes && logoTexture.parse(*logoBytes) && logoTexture.pixels[3] == 0,
                "logo aspect ratio preserved with transparent padding");
        require(texture.pixels[(256 * 512 + 64) * 4 + 2] > texture.pixels[(256 * 512 + 64) * 4],
                "import preserves red and blue channel order");
        mods::profileName("test driver");
        require(mods::imageName(false) == logo, "custom image survives profile reload");
        require(!mods::imageBytes("mod_image_../../outside"), "image lookup cannot escape profile directory");
        test::NullRenderDevice device;
        const auto language = *Language::byIndex(0);
        const auto savedNeon=mods::neon(car);
        mods::previewNeon(car,{0x214080,1,4});
        require(mods::neon(car).rgb==0x214080,"draft previews without saving");
        mods::clearNeonPreview();
        require(mods::neon(car).rgb==savedNeon.rgb,"cancel restores saved neon");
        if (argc == 3) {
            auto game = std::filesystem::absolute(argv[1]), overlay = std::filesystem::absolute(argv[2]);
            std::filesystem::current_path(game);
            GameFiles::setOverrideRoot(overlay);
            CrtRandom random;
            GameData data(game);
            std::string error;
            require(data.load(random, &error), "game data loads with overlay");
            mods::addLogoSlots(data.defs);
            scene::Engine engine(device, data.tables, 3, false);
            for (const auto &definition : data.defs.cars) {
                if (definition.parts[8].parts.empty())
                    continue;
                const auto *def = &definition;
                const auto &name = def->name;
                require(def != nullptr, "car exists");
                Car c = makeCar(def);
                int custom = mods::logoIndex(c.modelName());
                require(custom >= 0, "custom decal choice registered");
                c.body[8] = static_cast<uint8_t>(custom);
                auto model = scene::CarModel::load(engine, c, data.catalogue, random, true);
                require(model != nullptr, "custom decal model loads");
                const auto *mesh = model->slotMesh(scene::CarModel::kDecal);
                bool used = false;
                if (mesh)
                    for (auto &p : mesh->parts())
                        if (p.material && p.material->texture(0) == engine.texture(logo, "cartextures"))
                            used = true;
                if (!used && mesh)
                    for (auto &p : mesh->parts())
                        if (p.material)
                            std::cerr << "decal material class " << p.material->classId() << " on " << name << "\n";
                require(used, "uploaded logo reaches the car decal material");
                c.body[8] = 0;
                model->refitParts(c, data.catalogue);
                model->colourDecal(c);
                mesh = model->slotMesh(scene::CarModel::kDecal);
                bool stale = false;
                if (mesh)
                    for (auto &p : mesh->parts())
                        if (p.material && p.material->texture(0) == engine.texture(logo, "cartextures"))
                            stale = true;
                require(!stale, "switching to stock decal restores original texture");
            }
        }
        mods::configure({});
        GameFiles::setOverrideRoot({});
        std::filesystem::remove_all(folder);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(folder);
        return 1;
    }
}
