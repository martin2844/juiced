from pathlib import Path
import json,shutil,hashlib,html
from PIL import Image,ImageDraw,ImageFont
r=Path.cwd();e=r/'out/validation/peugeot405-rework-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405');sources=r/'tools/peugeot405-rework/assets';sources.mkdir()
for src,dest in [(w/'study08/Peugeot405-Juiced-Custom-Shop.blend','Peugeot405-Authoring.blend'),(w/'studio08/Peugeot405-Custom-Shop.blend','Peugeot405-Custom-Shop.blend'),(w/'study08/build-receipt.json','build-receipt.json')]:shutil.copyfile(src,sources/dest)
g=e/'gallery';g.mkdir();fontpath=next(Path('/nix/store').glob('*dejavu-fonts-*/share/fonts/truetype/DejaVuSans.ttf'));font=ImageFont.truetype(str(fontpath),20)
sheet=Image.new('RGB',(1440,1640),'#15171a');d=ImageDraw.Draw(sheet);d.text((22,14),'PEUGEOT 405 / Original - Street - Combat - Touring / Native Juiced',font=font,fill='white')
styles=[('stock','Original'),('street-blue','Street'),('combat-blue','Combat'),('touring-blue','Touring')]
for i,(style,title) in enumerate(styles):
 for j,angle in enumerate([135,45]):
  src=e/'final08'/f'PEUGEOT_405_GLX-{style}-yaw{angle}.png'
  with Image.open(src) as im:
   im=im.convert('RGB').crop((60,285,1220,820));im.thumbnail((710,360));sheet.paste(im,(j*720,i*400+50))
  d.text((j*720+18,i*400+411),title+' / '+('front' if j==0 else 'rear'),font=font,fill='white')
sheet.save(g/'overview.jpg',quality=96)
style='body{background:#12151a;color:#edf0f5;font:17px system-ui;max-width:1300px;margin:40px auto;padding:0 24px}h1{font-size:38px}h2{margin-top:48px}img{display:block;width:100%;height:auto;border-radius:9px}a{color:#79b7ff}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:18px}p{line-height:1.6;color:#c8ced8}'
parts=['<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Peugeot405 Custom Shop</title><style>'+style+'</style><h1>Peugeot405 Custom Shop</h1><p>Capturas del render nativo de Juiced. Tres conjuntos nuevos de paragolpes, faldones y alerones propios del405; capós con aberturas y lamas. R18, revisión16 de Wave2. Las escenas de Blender son editables; su iluminación y materiales son una aproximación al juego.</p><img src="overview.jpg" alt="Original y tres conjuntos nuevos, frente y cola">']
for style,title in styles[1:]+[('combat-red-vinyl8','Combat / rojo y vinilo8'),('touring-silver-vinyl3','Touring / plata y vinilo3')]:
 path='../final08/'+style+'-contact.jpg';parts+=['<h2>'+html.escape(title)+'</h2><p>Ocho ángulos de la misma configuración, cargada y cambiada por CarView.</p><a href="'+path+'"><img loading="lazy" src="'+path+'" alt="'+html.escape(title)+'"></a>']
parts+=['<h2>Archivos y límites</h2><p>El interior fuente sigue incompleto y está oculto por cristales opacos reflectantes. No se alteraron física, potencia, sonido, formato de saves ni los otros nueve autos. Las capturas prueban presentación y refit nativos; no son una medición de FPS ni una prueba física en la RP5. Los reflejos del piso conservan el comportamiento del render actual.</p>']
(g/'index.html').write_text('\n'.join(parts)+'\n')
manifest={p.relative_to(sources).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir() if p.is_file()};(e/'editable-sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Prepared gallery and two editable Blender files:',json.dumps(manifest,indent=2))
