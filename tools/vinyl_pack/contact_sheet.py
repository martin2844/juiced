from PIL import Image,ImageDraw
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--pattern',default='*.bmp');p.add_argument('--out',type=Path)
a=p.parse_args();files=sorted(a.folder.glob(a.pattern))
for start in range(0,len(files),24):
 subset=files[start:start+24];sheet=Image.new('RGB',(1280,((len(subset)+2)//3)*260),'#222222');d=ImageDraw.Draw(sheet)
 for i,f in enumerate(subset):
  im=Image.open(f).convert('RGB');im.thumbnail((425,235));x=i%3*426;y=i//3*260
  sheet.paste(im,(x,y));d.text((x+3,y+236),f.stem[-35:],fill='white')
 out=a.out or a.folder/'sheet.png';out=out.with_name(out.stem+str(start//24)+'.png');sheet.save(out)
 print(out)
