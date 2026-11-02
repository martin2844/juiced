"""Copy editable sources and make a local photo/native comparison gallery."""
from pathlib import Path
import hashlib,html,json,shutil
from PIL import Image,ImageDraw,ImageFont
r=Path.cwd();e=r/'out/validation/peugeot405-photo-kits-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-photo-kits');assets=r/'tools/peugeot405-photo-kits/assets';assets.mkdir(exist_ok=True)
for p in [w/'study05/Peugeot405-Photo-Kits-Authoring.blend',w/'studio05/Peugeot405-Photo-Kits.blend',w/'study05/build-receipt.json']:shutil.copyfile(p,assets/p.name)
g=e/'gallery';g.mkdir(exist_ok=True);fontpath=next(Path('/nix/store').glob('*dejavu-fonts-*/share/fonts/truetype/DejaVuSans.ttf'));font=ImageFont.truetype(str(fontpath),19)
sheet=Image.new('RGB',(1800,1420),'#15171b');d=ImageDraw.Draw(sheet);d.text((20,14),'405 / Referencia fotografica - Adaptacion Juiced: frente y cola',font=font,fill='white')
spec=json.loads((r/'tools/peugeot405-photo-kits/references.json').read_text());styles=['gutmann-blue','stratos-blue','showcase-blue'];photos=['gutmann-blue.jpg','white-kit.jpg','morette-2002.jpg']
for row,(kit,style,photo) in enumerate(zip(spec['kits'],styles,photos)):
 y=row*450+55
 with Image.open(e/'references'/photo) as im:
  im=im.convert('RGB');im.thumbnail((490,385));sheet.paste(im,(10,y))
 for j,angle in enumerate([135,45]):
  with Image.open(e/'final05'/f'PEUGEOT_405_GLX-{style}-yaw{angle}.png') as im:
   im=im.convert('RGB').crop((100,290,1190,800));im.thumbnail((640,385));sheet.paste(im,(510+j*645,y))
 d.text((15,y+390),kit['name']+' / slot '+str(kit['slot']),font=font,fill='white')
 d.text((520,y+390),'Frente / Juiced',font=font,fill='white');d.text((1170,y+390),'Cola / Juiced',font=font,fill='white')
sheet.save(g/'overview.jpg',quality=96)
style='body{background:#12151a;color:#edf0f5;font:17px system-ui;max-width:1400px;margin:40px auto;padding:0 24px}h1{font-size:38px}h2{margin-top:48px}img{display:block;width:100%;height:auto}a{color:#79b7ff}p{line-height:1.6;color:#c8ced8}'
parts=['<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>405 / Photo Kits r19</title><style>'+style+'</style><h1>Peugeot405 / Photo Kits r19</h1><p>Fotos de referencia y capturas reales del renderer de Juiced. Tres interpretaciones adaptadas a los encastres del coche existente; no son réplicas exactas. Wave2 revisión17. Instalación local de Linux.</p><img src="overview.jpg" alt="Fotos y tres kits en el renderer nativo">']
for kit,style in zip(spec['kits'],styles):
 parts+=['<h2>'+html.escape(kit['name'])+'</h2><p><a href="'+kit['url']+'">Fuente de las fotografías</a>. '+html.escape(kit['observed'])+'</p><p>'+html.escape(kit['adaptation'])+'</p><img loading="lazy" src="../final05/'+style+'-contact.jpg" alt="Ocho ángulos">']
for name,title in [('plain-carbon-silver','Capó de carbono / prensado original'),('stratos-red-vinyl8','Capó con toma / pintura y vinilo8'),('showcase-silver-vinyl3','Capó con toma / carbono y vinilo3'),('gutmann-silver','Gutmann / pintura plata'),('stock','Original sin kit')]:
 parts+=['<h2>'+title+'</h2><img loading="lazy" src="../final05/'+name+'-contact.jpg" alt="'+title+'">']
parts+=['<h2>Fuentes editables y límites</h2><p><a href="../../../../tools/peugeot405-photo-kits/assets/Peugeot405-Photo-Kits.blend">Blender: coche completo, cuatro escenas y texturas empaquetadas</a> · <a href="../../../../tools/peugeot405-photo-kits/assets/Peugeot405-Photo-Kits-Authoring.blend">Blender: construcción de las piezas</a></p><p>La cola del tercer coche no aparece en las fotos y es una adaptación propia. Se conserva el escape izquierdo y los faros originales. No cambian los alerones de r18, física, saves ni otros coches. El interior fuente sigue incompleto y oculto por cristales opacos. El reflejo del piso conserva sus defectos actuales. La galería comprueba presentación, no FPS ni conducción.</p>']
(g/'index.html').write_text('\n'.join(parts)+'\n')
(e/'editable-sources.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in assets.iterdir() if p.is_file()},indent=2)+'\n')
print(g/'index.html')
