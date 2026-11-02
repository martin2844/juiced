// Isolated native-render diagnostic. No user saves are loaded or written.
#include "Linux/Render/GlesDevice.h"
#include "Core/Data/GameData.h"
#include "Core/Cars/CarCollection.h"
#include "Game/Cars/CarView.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        auto game = std::filesystem::absolute(argv[1]);
        auto output = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(output);
        std::filesystem::current_path(game);
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        auto* window=SDL_CreateWindow("Car render isolation",1280,960,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window) throw std::runtime_error(SDL_GetError());
        {
            oj::CrtRandom random;
            oj::GameData data(game);
            std::string error;
            if(!data.load(random,&error)) throw std::runtime_error(error);
            oj::render::DeviceDesc desc;desc.window=reinterpret_cast<HWND>(window);
            oj::render::GlesDevice gpu(desc);
            oj::scene::Engine engine(gpu,data.tables,3,false);
            oj::ui::Screen screen(gpu,*oj::Language::byName("english"));
            screen.setView3dFactory([&](const oj::ui::Element& e)->std::unique_ptr<oj::ui::View3d>{
                return std::make_unique<oj::scene::View3d>(engine,e,random);
            });
            if(!screen.load("instant_selcar")) throw std::runtime_error("selector load failed");
            screen.setHidden("Locked",true);screen.setHidden("Locked_Text",true);
            screen.setHidden("CARFADE",true);
            screen.hideContaining("MODDING");screen.hideContaining("CARLOGO");
            screen.hideContaining("CARORIGIN");screen.hideContaining("PROT_");
            oj::Car car=oj::makeCar(data.defs.byModel("PUNTO_STOCK"));
            if(!car.def) throw std::runtime_error("Punto missing");
            oj::CarMaking making{data.defs,data.catalogue,random,data.autoModifyCounter};
            oj::autoModify(car,false,2,making);
            car.base=0xffefbd0e;car.neon=0xff00ff00;car.body[oj::kNeon]=0;
            oj::scene::CarView::Setup setup;
            setup.element="FRAME_CAR3D";setup.zoom=.7f;setup.mirror=true;setup.shadow=true;setup.keepFolder=true;
            oj::scene::CarView view(engine,screen,setup,data.catalogue,data.tables);
            view.show(car,nullptr,0,.785398f,0,false,random);
            auto* scene=dynamic_cast<oj::scene::View3d*>(screen.view("FRAME_CAR3D"));
            if(!scene) throw std::runtime_error("scene absent");
            scene->setMode(oj::scene::MoveMode::Map,nullptr);scene->autoSpeed=0;
            screen.finishTransitions();
            auto settle=[&]{
                for(int i=0;i<3000;++i){gpu.letLoadingRun();view.frame(1.f/60,random);view.finish(1.f/60,random);
                    if(view.hasModel()&&!view.busy()) return;
                    SDL_Delay(1);
                }
                throw std::runtime_error("car loading timed out");
            };
            settle();
            scene->setZoom(.7f);scene->setZoomTarget(.7f);
            view.repaint(car);
            auto capture=[&](const char* name){
                // Camera/zoom interpolation advances per frame even at dt=0.
                // Converge before each capture; repeats must be pixel-identical.
                for(int i=0;i<1000;++i){gpu.beginFrame();gpu.clear(3,0,1,0);screen.update(0);screen.draw(0,0);gpu.endFrame();}
                if(!gpu.capture((output/name).wstring())) throw std::runtime_error("capture failed");
                std::cout<<"CAPTURE "<<name<<'\n';
            };
            capture("neon-on.bmp");
            car.neon=0;view.repaint(car);
            capture("neon-transparent.bmp");
            car.neon=0xff00ff00;view.repaint(car);
            capture("neon-on-repeat.bmp");
            // CarView::attach registers mirror, real car, shadow, neon in that
            // order. Preserve nodes/transforms and vary only draw submission.
            if(scene->worldDraws.size()!=4) throw std::runtime_error("unexpected CarView draw layout");
            const auto draws=scene->worldDraws;
            scene->worldDraws={draws[0],draws[1],draws[2]};
            capture("no-neon-draw.bmp");
            scene->worldDraws={draws[0],draws[1]};
            capture("mirror-car-only.bmp");
            scene->worldDraws={draws[1],draws[2]};
            capture("car-shadow-only.bmp");
            scene->worldDraws={draws[1]};
            capture("car-only.bmp");
            scene->worldDraws=draws;
            capture("all-restored.bmp");
        }
        SDL_DestroyWindow(window);SDL_Quit();return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';SDL_Quit();return 1;}
}
