// Make a deliberately stale-rated prototype fixture in validation storage only.
#include "Core/Data/GameData.h"
#include "Core/Saves/Save.h"
#include <fstream>
#include <cstring>
#include <iostream>
int main(int argc,char**argv){
 if(argc!=5)return 2;
 const auto game=std::filesystem::absolute(argv[1]),overlay=std::filesystem::absolute(argv[2]);
 const auto source=std::filesystem::absolute(argv[3]),dest=std::filesystem::absolute(argv[4]);
 if(dest.string().find("/out/validation/peugeot405-refinement-20261101/")==std::string::npos || std::filesystem::exists(dest))return 2;
 std::ifstream in(source,std::ios::binary);if(!in)return 1;std::vector<char>raw((std::istreambuf_iterator<char>(in)),{});std::vector<std::byte>bytes(raw.size());std::memcpy(bytes.data(),raw.data(),raw.size());
 oj::save::Archive archive;std::string error;if(!archive.read(bytes,&error))return 1;
 oj::GameFiles::setOverrideRoot(overlay);std::filesystem::current_path(game);oj::CrtRandom random;oj::GameData data(game);if(!data.load(random,&error)||!data.career)return 1;
 auto&career=*data.career;if(!career.read(archive.entries.at("CMSGlobal.sav"),&error))return 1;
 bool found=false;
 for(auto&car:career.garage)if(car.modelName()=="PEUGEOT_405_GLX_STOCK"){
  for(size_t kind=0;kind<oj::kModKinds;++kind){const auto&parts=data.catalogue.lists[kind].parts;
   for(size_t i=0;i<parts.size();++i)if(parts[i].level==3){car.mods[kind]=static_cast<uint8_t>(i);break;}}
  car.rating=oj::Car::bitsOf(216.1844f);car.torqueFactor=oj::Car::bitsOf(1.0f);found=true;break;
 }
 if(!found)return 1;archive.entries["CMSGlobal.sav"]=career.write();const auto output=archive.write();
 std::ofstream out(dest,std::ios::binary);out.write(reinterpret_cast<const char*>(output.data()),output.size());if(!out)return 1;
 std::cout<<"PRIVATE FIXTURE: installed prototype parts, stale216.1844 rating. Real profiles untouched.\n";
}
