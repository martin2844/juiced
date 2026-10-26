#include "Core/Base/GameFiles.h"
#include "Core/Base/Text.h"
#include "Core/Data/GameData.h"
#include "Core/Cars/CarPhysics.h"
#include "Game/Cars/CarView.h"
#include "Android/Render/GlesDevice.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Fakes/FakeGarageHost.h"
#include "Game/Frontend/Garage/Customise/Styling.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace oj;
    if (argc < 3) return 2;
    try {
        const auto game=std::filesystem::absolute(argv[1]), overlay=std::filesystem::absolute(argv[2]);
        const auto captures=argc>3?std::filesystem::absolute(argv[3]):std::filesystem::path{};
        std::filesystem::current_path(game);
        GameFiles::setOverrideRoot(overlay);
        CrtRandom random; GameData data(game); std::string error;
        if (!data.load(random,&error)) throw std::runtime_error(error);
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
            bool checkedMenu=false;
            for (size_t i=0;i<def.parts[kDecal].parts.size();++i) {
                const auto& part=def.parts[kDecal].parts[i];
                if (!part.model.starts_with("DECAL_NFS_")) continue;
                auto car=makeCar(&def);
                car.body[kDecal]=uint8_t(i);car.base=0xffcccccc;car.decalColour=0xffffffff;
                if (argc>4 && part.model!=argv[4]) continue;
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
                if (!checkedMenu) {
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
