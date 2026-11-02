// Read-only native tuning diagnostic; no personal profiles or game writes.
#include "Core/Data/GameData.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Physics/CarBody.h"
#include "Framework/Fakes/FlatGround.h"
#include <iostream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cmath>
int main(int argc,char**argv) {
 if(argc<3||argc>4)return 2;
 const auto game=std::filesystem::absolute(argv[1]);
 oj::GameFiles::setOverrideRoot(std::filesystem::absolute(argv[2]));
 std::filesystem::current_path(game);oj::CrtRandom random;oj::GameData data(game);std::string error;
 if(!data.load(random,&error)){std::cerr<<error;return 1;}
 std::cout<<"model,level,bhp,rating,displayClass,turbo,torque,exhaustValue,inductionValue\n"<<std::fixed<<std::setprecision(4);
 for(const int id:std::array<int,4>{57,0,1,2}) {
  const auto*def=data.defs.byId(id);if(!def)continue;const auto*d=data.dynamics->of(*def);if(!d)return 1;
  if(id==57) { std::cerr<<"SCALES";for(float scale:d->turboUpgradeScale)std::cerr<<' '<<scale;std::cerr<<'\n';
   const auto raw=oj::GameFiles(game).read("scripts/dynamic_peugeot_405_glx.txt");
   if(raw){auto direct=oj::parseDynamics(std::string_view(reinterpret_cast<const char*>(raw->data()),raw->size()));std::cerr<<"DIRECT";for(float scale:direct->turboUpgradeScale)std::cerr<<' '<<scale;std::cerr<<'\n';} }
  for(int level=-1;level<=3;++level){
   auto car=oj::makeCar(def);float exhaust=1,induction=1;
   if(level>=0) for(size_t kind=0;kind<oj::kModKinds;++kind){
    const auto&parts=data.catalogue.lists[kind].parts;int best=-1;
    for(size_t i=0;i<parts.size();++i)if(parts[i].level==level&&(best<0||parts[i].dataValue>parts[best].dataValue))best=static_cast<int>(i);
    if(best>=0){car.mods[kind]=static_cast<uint8_t>(best);if(kind==oj::kExhaust)exhaust=parts[best].dataValue;if(kind==oj::kInduction)induction=parts[best].dataValue;}
   }
   auto tuned=*d;oj::fitParts(tuned,car,data.catalogue);const float rating=oj::rateCar(car,d,data.catalogue);
   std::cout<<def->name<<','<<level<<','<<oj::peakPower(tuned)/2.8f/745.7f<<','<<rating<<','<<oj::carClass(car)+1<<','<<tuned.turbo<<','<<tuned.maxTorque<<','<<exhaust<<','<<induction<<'\n';
   if(!std::isfinite(rating))return 1;
  }
 }
 if(argc==4) {
  const auto*def=data.defs.byId(57);const auto*d=data.dynamics->of(*def);
  const std::array<oj::Vec4,4> bases{{{-76.0732f,31.6654f,133.45f,0},{75.9722f,31.6655f,133.4498f,0},
                                     {-76.0732f,31.6654f,-133.45f,0},{75.9722f,31.6655f,-133.4502f,0}}};
  std::cout<<"DRIVE,level,bhp,seconds100,seconds200,speed10,speed20,speed30,asrFrames\n";
  for(int level:std::array<int,3>{-1,2,3}) {
   auto car=oj::makeCar(def);
   if(level>=0)for(size_t kind=0;kind<oj::kModKinds;++kind){
    const auto&parts=data.catalogue.lists[kind].parts;
    for(size_t i=0;i<parts.size();++i)if(parts[i].level==level){car.mods[kind]=static_cast<uint8_t>(i);break;}
   }
   auto fitted=*d;oj::fitParts(fitted,car,data.catalogue);oj::CarBody body(fitted,{});
   body.weighAxles(bases[0][2],bases[2][2]);body.raceTune(false,data.tables.modelClasses[57]);
   body.restWheels(bases);body.setAsr(true);oj::test::FlatGround flat;oj::ContactPools pools;
   body.collision=&flat;body.pools=&pools;body.setGear(1);body.state[body.engineIndex]=d->torqueSpeeds.front();body.setState(body.state);
   for(int i=0;i<180;++i){pools.empty();body.step(1.f/60);}
   body.setGear(2);body.automatic=true;float t100=-1,t200=-1,v10=0,v20=0,v=0;int cuts=0;
   for(int i=0;i<1800;++i){
    body.throttle=1;body.steering=0;pools.empty();body.step(1.f/60);v=std::hypot(body.velocity[0],body.velocity[2])*3.6f;
    if(!std::isfinite(v))return 1;if(t100<0&&v>=100)t100=(i+1)/60.f;if(t200<0&&v>=200)t200=(i+1)/60.f;
    if(i==599)v10=v;if(i==1199)v20=v;cuts+=body.asrActive;
   }
   std::cout<<"DRIVE,"<<level<<','<<oj::peakPower(fitted)/2.8f/745.7f<<','<<t100<<','<<t200<<','<<v10<<','<<v20<<','<<v<<','<<cuts<<'\n';
  }
 }
}
