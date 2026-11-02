#include "Core/Base/GameFiles.h"
#include "Core/Base/Text.h"
#include "Core/Data/GameData.h"
#include "Core/Cars/CarPhysics.h"
#include "Game/Cars/CarView.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "Linux/Render/GlesDevice.h"
#else
#include "Android/Render/GlesDevice.h"
#endif
#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Fakes/FakeGarageHost.h"
#include "Game/Frontend/Garage/Customise/Styling.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace oj;
    if (argc < 3) return 2;
    try {
        const auto game=std::filesystem::absolute(argv[1]), overlay=std::filesystem::absolute(argv[2]);
        const auto captures=argc>3?std::filesystem::absolute(argv[3]):std::filesystem::path{};
        std::filesystem::current_path(game);
        std::vector<CarDef> baseline;
        if (const char* path=std::getenv("OJ_VINYL_BASELINE")) {
            GameFiles::setOverrideRoot(std::filesystem::absolute(path));
            CrtRandom random; GameData original(game); std::string error;
            if (!original.load(random,&error)) throw std::runtime_error(error);
            baseline=original.defs.cars;
        }
        GameFiles::setOverrideRoot(overlay);
        CrtRandom random; GameData data(game); std::string error;
        if (!data.load(random,&error)) throw std::runtime_error(error);
        if (!baseline.empty()) {
            if (baseline.size()!=data.defs.cars.size()) throw std::runtime_error("Vinyl pack changed car roster");
            for (size_t c=0;c<baseline.size();++c) {
                const auto& before=baseline[c]; const auto& after=data.defs.cars[c];
                if (before.name!=after.name || before.id!=after.id || before.model!=after.model)
                    throw std::runtime_error("Vinyl pack changed saved car identifiers");
                for (int k=0;k<kCarPartCategories;++k) {
                    const auto& a=before.parts[k]; const auto& b=after.parts[k];
                    if (b.parts.size()<a.parts.size() || a.allowNone!=b.allowNone || a.standard!=b.standard ||
                        (k!=kDecal && a.parts.size()!=b.parts.size()))
                        throw std::runtime_error("Vinyl pack changed existing part catalogue");
                    for (size_t i=0;i<a.parts.size();++i)
                        if (a.parts[i].model!=b.parts[i].model || a.parts[i].index!=b.parts[i].index ||
                            a.parts[i].label!=b.parts[i].label || a.parts[i].brand!=b.parts[i].brand)
                            throw std::runtime_error("Vinyl pack changed an existing saved part index");
                }
            }
            std::cout<<"PASS existing car/part identifiers and indices preserved\n";
        }
        test::NullRenderDevice nullDevice;
        std::unique_ptr<render::GlesDevice> gpu;
        if (!captures.empty()) {
            std::filesystem::create_directories(captures);
            SDL_Init(SDL_INIT_VIDEO);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
            auto* window=SDL_CreateWindow("Vinyl pack validation",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
            render::DeviceDesc desc;desc.window=reinterpret_cast<HWND>(window);
            gpu=std::make_unique<render::GlesDevice>(desc);
        }
        render::IRenderDevice& device=gpu?static_cast<render::IRenderDevice&>(*gpu):nullDevice;
        scene::Engine engine(device,data.tables,3,false);
        ui::Screen screen(device,*Language::byName("english"));
        screen.setView3dFactory([&](const ui::Element& e)->std::unique_ptr<ui::View3d>{
            return std::make_unique<scene::View3d>(engine,e,random);
        });
        if (gpu) {
            if (!screen.load("carsel")) throw std::runtime_error("carsel screen");
            for (auto name:{"menu","backmenu","infobox","aniline","animline2","menufade"}) screen.setHidden(name,true);
            screen.finishTransitions();
        }
        int count=0;
        for (const auto& def:data.defs.cars) {
            // Optional final car filter keeps a whole-pack visual review bounded;
            // omit it for the separate all-car load/save validation.
            if (argc > 6 && def.name != argv[6]) continue;
            bool checkedMenu=false;
            for (size_t i=0;i<def.parts[kDecal].parts.size();++i) {
                const auto& part=def.parts[kDecal].parts[i];
                if (!part.model.starts_with("DECAL_NFS_")) continue;
                auto car=makeCar(&def);
                car.body[kDecal]=uint8_t(i);car.base=0xffcccccc;car.decalColour=0xffffffff;
                if (argc>5) car.base=0xff000000u|static_cast<uint32_t>(std::stoul(argv[5],nullptr,16));
                if (argc>4 && argv[4][0] != '\0' && part.model!=argv[4]) continue;
                for (bool garage:{false,true}) {
                    auto model=scene::CarModel::load(engine,car,data.catalogue,random,garage);
                    if (!model || !model->damagePoints("DECAL") || model->damagePoints("DECAL")->empty())
                        throw std::runtime_error("Missing decal geometry: "+part.model);
                    const auto* mesh=model->slotMesh(scene::CarModel::kDecal);
                    if (!mesh || mesh->parts().empty()) throw std::runtime_error("Empty decal mesh");
                    for (const auto& p:mesh->parts())
                        if (!p.material || !p.material->texture(0))
                            throw std::runtime_error("Missing vinyl material/texture: "+part.model);
                }
                Career saved; saved.garage.push_back(car); Career restored;
                if (!restored.read(saved.write(),&error) || restored.garage[0].body[kDecal]!=i)
                    throw std::runtime_error("Vinyl index save round trip");
                ++count;
                ui::Screen menu(device,*Language::byName("english"));
                {
                    if (!menu.load("decalmenu")) throw std::runtime_error("decalmenu screen");
                    test::FakeGarageHost host;host.data=&data;host.base=&screen;
                    host.texts.load("scripts/english.txt");
                    data.career->garage={car};data.career->dealer.chosen=0;
                    frontend::DecalMenu window;window.enter(host,menu);
                    if (menu.menuSize("Menu")<def.parts[kDecal].parts.size() ||
                        menu.menuItemText("Menu",i)!=toWide(part.label.substr(0,part.label.find(':'))+" "+part.model.substr(part.model.size()-4)))
                        throw std::runtime_error("Imported vinyl missing from workshop menu");
                    menu.selectMenuItem("Menu",i);menu.finishTransitions();
                }
                if (gpu) {
                    scene::GarageView view(engine,screen,"FRAME_CAR3D",data.catalogue);
                    view.enterSel(car,random);
                    view.mapMode();
                    for (int angle=0;angle<3;++angle) {
                        view.setRig(-.24f,angle==0?1.57f:angle==1?-1.57f:3.6f);
                        for (int frame=0;frame<30;++frame) {
                            gpu->beginFrame();engine.frameParameters();screen.update(1.f/60);
                            screen.draw(frame/60.f,1.f/60);gpu->endFrame();
                        }
                        if (!gpu->capture((captures/(def.name+"_"+part.model+"_"+std::to_string(angle)+".bmp")).wstring()))
                            throw std::runtime_error("Capture failed");
                    }
                    if (!checkedMenu) {
                        for (int frame=0;frame<30;++frame) {
                            gpu->beginFrame();engine.frameParameters();
                            screen.draw(frame/60.f,1.f/60);menu.update(1.f/60);menu.draw(frame/60.f,1.f/60);gpu->endFrame();
                        }
                        if (!gpu->capture((captures/(def.name+"_menu.bmp")).wstring()))
                            throw std::runtime_error("Menu capture failed");
                    }
                }
                checkedMenu=true;
                std::cout<<"PASS "<<def.name<<" "<<i<<" "<<part.model<<" "<<part.label<<std::endl;
            }
        }
        if (!count) throw std::runtime_error("No imported vinyls");
        std::cout<<"Validated "<<count<<" vinyls in garage/race plus save roundtrip\n";
    } catch (const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
