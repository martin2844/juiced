"""Prepare the local native before/after gallery and editable Blender sources."""
from pathlib import Path
import shutil,json,hashlib,html
from PIL import Image,ImageDraw,ImageFont
r=Path.cwd();e=r/'out/validation/peugeot405-refinement-20261101';old=r/'out/validation/peugeot405-photo-kits-20261101/final05';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-refinement');assets=r/'tools/peugeot405-refinement/assets';assets.mkdir(exist_ok=True)
for p in [w/'study04/Peugeot405-Refinement-Authoring.blend',w/'studio04/Peugeot405-Refined.blend',w/'study04/build-receipt.json']:shutil.copyfile(p,assets/p.name)
g=e/'gallery';g.mkdir(exist_ok=True);fontpath=next(Path('/nix/store').glob('*dejavu-fonts-*/share/fonts/truetype/DejaVuSans.ttf'));font=ImageFont.truetype(str(fontpath),20)
sheet=Image.new('RGB',(1800,1390),'#15171a');d=ImageDraw.Draw(sheet);d.text((18,15),'405 / r19 anterior - r20 refinado / Capturas nativas de Juiced',font=font,fill='white')
styles=[('gutmann-silver','Gutmann / lip bajo'),('stratos-red-vinyl8','Stratos / puente de epoca'),('showcase-silver-vinyl3','Showcase / GT pintado')]
for row,(state,title) in enumerate(styles):
 for col,(folder,angle,label) in enumerate([(old,45,'Anterior r19'),(e/'final04',45,'Refinado r20'),(e/'final04',0,'Refinado r20 / cola')]):
  path=folder/f'PEUGEOT_405_GLX-{state}-yaw{angle}.png'
  with Image.open(path) as im:
   im=im.convert('RGB').crop((80,275,1220,850));im.thumbnail((590,370));sheet.paste(im,(col*600,row*440+55))
  d.text((col*600+12,row*440+400),label,font=font,fill='white')
 d.text((12,row*440+430),title,font=font,fill='#8bc2ff')
sheet.save(g/'rear-before-after.jpg',quality=96)
css='body{background:#12151a;color:#edf0f5;font:17px system-ui;max-width:1450px;margin:40px auto;padding:0 24px}h1{font-size:38px}h2{margin-top:42px}img{display:block;width:100%;height:auto}p{line-height:1.55;color:#c8ced8}a{color:#80bfff}td,th{padding:10px 28px;text-align:left;border-bottom:1px solid #35404c}table{border-collapse:collapse}'
parts=['<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>405 / Refinamiento r20</title><style>'+css+'</style><h1>Peugeot405 / Refinamiento r20</h1><p>Instalado en Linux: Wave2 revisión18 y motor nativo actualizado. Traseros continuos, tres alerones nuevos y parrilla con cavidad opaca. Se conservan los frentes, capós y faldones de r19.</p><img src="rear-before-after.jpg" alt="Comparación nativa de traseros y alerones">']
for state,title in styles:
 parts+=['<h2>'+title+'</h2><img loading="lazy" src="../final04/'+state+'-contact.jpg" alt="Ocho ángulos nativos">']
parts+=['<h2>Parrilla: vista cercana</h2><p>Frente y diagonales con el renderer real, coche de fábrica y los tres kits. Las lamas mantienen su forma y tienen un fondo opaco. La comprobación geométrica respeta el descarte de caras traseras:126 rayos interceptados;66 quedaban abiertos en r19.</p><img loading="lazy" src="../grille-native/contact.jpg" alt="Doce primeros planos de la parrilla">','<h2>Preparación especial aprobada</h2><p>Valores obtenidos cargando las definiciones y piezas reales del juego. La misma preparación se aplica al motor de carrera y al cálculo de clase. Los otros modelos conservan el catálogo original.</p><table><tr><th>Conjunto</th><th>BHP medidos</th><th>Clase</th></tr>']
perf=json.loads((e/'performance-receipt.json').read_text())
for row,label in zip(perf['measured'],['Fábrica','Street','Sports','Competition','Prototype']):parts+=['<tr><td>'+label+'</td><td>'+str(round(float(row['bhp']),1))+'</td><td>'+row['displayClass']+'</td></tr>']
parts+=['</table><p>Los autos ya equipados actualizan su clasificación al cargar el perfil. No se agregan piezas ni cambia la identidad del auto. Se verificaron415 pruebas unitarias, los controles nativos y1.322 comprobaciones de carga/refit. Las pruebas de física en piso plano son diagnósticos; no equivalen a prestaciones reales ni a una medición de FPS.</p><h2>Editables</h2><p><a href="../../../../tools/peugeot405-refinement/assets/Peugeot405-Refined.blend">Blender: coche completo y cuatro escenas</a> · <a href="../../../../tools/peugeot405-refinement/assets/Peugeot405-Refinement-Authoring.blend">Blender: construcción de piezas</a></p><p>Se conservan la cabina fuente incompleta y el cristal opaco existente, así como los defectos de reflejos del renderer. Este paquete no actualiza por sí solo el ejecutable de Android o macOS. Referencias y atribución continúan en el <a href="../../peugeot405-photo-kits-20261101/gallery/index.html">estudio fotográfico r19</a>.</p>']
(g/'index.html').write_text('\n'.join(parts)+'\n');(e/'editable-sources.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in assets.iterdir() if p.is_file()},indent=2)+'\n');print(g/'index.html')
