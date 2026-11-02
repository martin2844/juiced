"""Build a private CarView review harness; does not relink the game executable."""
from pathlib import Path
import argparse,hashlib,json,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('out',type=Path);a=p.parse_args();r=Path.cwd();e=a.out.resolve();e.mkdir(parents=True,exist_ok=True);b=r/'out/linux'
s=(r/'tools/CarVisualGallery.cpp').read_text();s=s.replace('uint8_t spoiler = 0;','uint8_t spoiler = 0; uint32_t paint = 0xff1950a5;')
old='states = {{"stock", 0, 0x7f, false}, {"kit1-carbon-vinyl8", 1, 8, false, 1},\n                              {"kit2-carbon-vinyl8", 1, 8, false, 2}, {"kit3-carbon-vinyl8", 1, 8, false, 3}};'
new='''states = {{"stock",0,0x7f,false,0,0},
 {"gutmann-blue",0,0x7f,false,1,1},
 {"stratos-blue",2,0x7f,false,2,2},
 {"showcase-blue",3,0x7f,false,3,3},
 {"stratos-red-vinyl8",2,8,false,2,2,0xffb32618},
 {"showcase-silver-vinyl3",3,3,false,3,3,0xffb8bec4},
 {"plain-carbon-silver",1,0x7f,false,0,0,0xffb8bec4},
 {"gutmann-silver",0,0x7f,false,1,1,0xffb8bec4}};'''
assert old in s;s=s.replace(old,new).replace('if (wingSweep) car.body[oj::kSpoiler] = state.spoiler;','if (wingSweep || kitSweep) car.body[oj::kSpoiler] = state.spoiler;\n                    car.base = state.paint;')
s=s.replace('closeReview ? std::vector<int>{0, 2, 4, 6}','closeReview ? std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}')
s=s.replace('closeReview ? 520.f : 800.f','closeReview ? 620.f : 800.f').replace('closeReview ? 160.f : 245.f','closeReview ? 185.f : 245.f')
source=e/'Peugeot405Shop.cpp';source.write_text(s)
entry=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/tools/CarVisualGallery.cpp'));args=shlex.split(entry['command']);obj=e/'Peugeot405Shop.o';exe=e/'peugeot405_shop';args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(source)
with (e/'build.log').open('w') as log:
 subprocess.run(args,cwd=b,stdout=log,stderr=subprocess.STDOUT,check=True)
 command=subprocess.check_output(['ninja','-C',str(b),'-t','commands','juiced_car_gallery'],text=True).splitlines()[-1];assert command.startswith(': && ') and command.endswith(' && :');link=shlex.split(command[5:-5]);link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link=[str(obj) if x=='CMakeFiles/juiced_car_gallery.dir/tools/CarVisualGallery.cpp.o' else x for x in link];link[link.index('-o')+1]=str(exe);subprocess.run(link,cwd=b,stdout=log,stderr=subprocess.STDOUT,check=True)
(e/'build-receipt.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),production_executable_changed=False),indent=2)+'\n');print(exe)
