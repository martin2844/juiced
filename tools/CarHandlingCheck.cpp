// Deterministic flat-ground diagnostic. Synthetic cars only; never reads/writes saves.
#include "Core/Data/GameData.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Physics/CarBody.h"
#include "Game/Cars/CarModel.h"
#include "Framework/Fakes/FlatGround.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include <cmath>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) return 2;
    const int fps = argc == 4 ? std::stoi(argv[3]) : 60;
    if (fps < 30 || fps > 240) return 2;
    const float dt = 1.f / fps;
    auto game = std::filesystem::absolute(argv[1]);
    oj::GameFiles::setOverrideRoot(std::filesystem::absolute(argv[2]));
    std::filesystem::current_path(game);
    oj::CrtRandom random;
    oj::GameData data(game);
    std::string error;
    if (!data.load(random, &error)) { std::cerr << error; return 1; }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    std::cout << "car,steer,asr,seconds100,seconds200,speed10,speed20,maxSlip,asrFrames,maxYaw,finalGear\n";
    for (const auto& def : data.defs.cars) {
        if (def.name != "FERRARI_458" && def.name != "VIPER" && def.name != "FALCON" && def.name != "NSX" && def.id < 54) continue;
        auto stock = oj::makeCar(&def);
        auto model = oj::scene::CarModel::load(engine, stock, data.catalogue, random, false);
        const auto* dynamics = data.dynamics->of(def);
        if (!model || !dynamics) return 1;
        std::array<oj::Vec4, 4> bases{};
        for (size_t i=0;i<4;++i) { auto* n=model->wheelNode(i); if (!n) return 1;
            bases[i]={n->local[3],n->local[7],n->local[11],0}; }
        for (float steer : {0.f, .15f}) for (bool asr : {true,false}) {
            auto fitted = *dynamics;
            oj::fitParts(fitted, stock, data.catalogue);
            oj::CarBody body(fitted, {});
            body.weighAxles(bases[0][2],bases[2][2]);
            body.raceTune(false, data.tables.modelClasses[def.id]);
            body.restWheels(bases);
            body.setAsr(asr);
            oj::test::FlatGround flat;
            oj::ContactPools pools;
            body.collision=&flat; body.pools=&pools;
            body.setGear(1);
            body.state[body.engineIndex]=dynamics->torqueSpeeds.front();
            body.setState(body.state);
            for(int i=0;i<3*fps;++i) { pools.empty(); body.step(dt); }
            body.setGear(2); body.automatic=true;
            float t100=-1,t200=-1,v10=0,v20=0,maxSlip=0,maxYaw=0;
            int cuts=0;
            for(int i=0;i<30*fps;++i) {
                body.throttle=1; body.steering=(i>=5*fps && i<7*fps)?steer:0;
                pools.empty(); body.step(dt);
                float v=std::hypot(body.velocity[0],body.velocity[2])*3.6f;
                if (!std::isfinite(v)) return 1;
                if (t100<0 && v>=100) t100=(i+1)*dt;
                if (t200<0 && v>=200) t200=(i+1)*dt;
                if (i==10*fps-1) v10=v; if (i==20*fps-1) v20=v;
                if (v>30) maxSlip=std::max(maxSlip,std::abs(std::atan2(body.localVelocity[0],body.localVelocity[2])));
                maxYaw=std::max(maxYaw,std::abs(body.angles[1]));
                cuts+=body.asrActive;
            }
            std::cout << def.name << ',' << steer << ',' << asr << ',' << t100 << ',' << t200 << ',' << v10 << ',' << v20 << ',' << maxSlip << ',' << cuts << ',' << maxYaw << ',' << body.gear << '\n';
        }
    }
}
