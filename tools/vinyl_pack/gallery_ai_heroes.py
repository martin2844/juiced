"""Export actual AI-livery runtime captures into a browsable review gallery."""
import argparse
import html
import json
from pathlib import Path
from PIL import Image


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for arg in ('package','captures','output'):
        ap.add_argument('--'+arg,type=Path,required=True)
    a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    rows=json.loads((a.package/'catalogue.json').read_text());cards=[]
    for row in rows:
        stem=row['car']+'_'+row['mesh']
        strip=Image.new('RGB',(1920,360))
        for angle in range(3):
            with Image.open(a.captures/f'{stem}_{angle}.bmp') as im:
                strip.paste(im.convert('RGB').resize((640,360)),(angle*640,0))
        strip.save(a.output/(stem+'.jpg'),quality=92)
        label=html.escape(row['car']+' · '+row['label'])
        cards.append(f'<article><h2>{label}</h2><a href="{stem}.jpg">'
                     f'<img loading="lazy" src="{stem}.jpg" alt="{label}: both sides and front angle"></a></article>')
    (a.output/'index.html').write_text('''<!doctype html><html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>AI Hero Liveries — capturas</title>
<style>body{background:#151619;color:#eee;font:16px system-ui;max-width:1500px;margin:32px auto;padding:20px}
img{width:100%;height:auto}h2{font-size:18px}article{border-top:1px solid #444;padding:24px 0}
input{font:inherit;padding:12px;width:90%;background:#25262a;color:white;border:1px solid #777}
a{color:#9fcdff}</style><h1>AI Hero Liveries</h1>
<p>Cinco diseños nuevos sobre las mallas completas de Juiced. Capturas reales del motor,
ambos laterales y vista oblicua; pintura gris para comparar. No son renders generados por IA.</p>
<p>Vista previa: 52 coches originales, 260 combinaciones. La galería permite revisar
los resultados; no implica aprobación visual de cada combinación. Android, daños y kits pendientes.</p>
<input aria-label="Filtrar coche o diseño" placeholder="Filtrar: CIVIC_97, Rachel, Supra…"
oninput="for(const c of document.querySelectorAll('article')) c.hidden=!c.textContent.toLowerCase().includes(this.value.toLowerCase())">
'''+''.join(cards)+'</html>')
    print(len(rows),'gallery entries')


if __name__=='__main__':main()
