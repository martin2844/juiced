"""Package a validated local conversion with attribution and a screenshot gallery."""
from pathlib import Path
import argparse,collections,hashlib,html,json,shutil,zipfile
from PIL import Image

SOURCES={
 'nd4':('ND4SPDWorld vinyl collection','https://www.nd4spdworld.com/blog/index.php/feat/carpack-3/','nd4.rar'),
 'fnf':('Haris Ahmad collection / individual authors in readmes','https://www.nfs-cars.com/need-for-speed-underground-2/6/files/view/11449/','fnf.rar'),
 'aj':('AJ_Lethal','https://www.nfsaddons.com/downloads/nfsug2/tools/2998/vinyl-pack.html','aj-pack.zip'),
}

def main():
 p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True)
 p.add_argument('--sources',type=Path,required=True);p.add_argument('--captures',type=Path,required=True)
 p.add_argument('--output',type=Path,required=True)
 p.add_argument('--version',default='preview1');p.add_argument('--inventory',type=Path)
 a=p.parse_args()
 if not all(c.isalnum() or c=='-' for c in a.version):raise ValueError('Invalid version')
 if a.output.exists():raise ValueError('Output must be new')
 a.output.mkdir(parents=True);dest=a.output/'underground-vinyls'
 shutil.copytree(a.package,dest)
 rows=json.loads((dest/'catalogue.json').read_text())
 (dest/'disabled').touch()
 shutil.copyfile(Path(__file__).with_name('README.md'),dest/'CONVERSION.md')
 source_count=sum(len(r.get('layers',[r])) for r in rows)
 composite_count=sum('layers' in r for r in rows)
 notes=['# Underground & F&F Vinyl Collection — '+a.version,'',
        f'{len(rows)} selectable options from {source_count} source layers, across {len(set(r["car"] for r in rows))} cars; {composite_count} composed designs.',
        '', 'Requires the native vinyl-catalogue extension (Android .9-vinyl-preview).',
        'Import this ZIP through the launcher Mods importer. It starts disabled.',
        'Enabling changes the catalogue save variant; disabling returns to your previous careers.',
        'No existing saves are migrated or deleted. Do not copy Custom Logo careers between variants.',
        '', 'Use stock body parts. Body kits, damage, recolouring and on-device performance remain unverified.',
        'Known multi-layer designs are composed. Independent hood, stripe and badge variants remain selectable.',
        'Some layers contain only hood or roof art. The preview gallery includes three views per option.',
        '', 'Sources and original credits:']
 provenance=[]
 for key,(author,url,archive) in SOURCES.items():
  notes.append(f'- {author}: {url}')
  source=a.sources/archive
  provenance.append(dict(pack=key,credit=author,url=url,archive=archive,sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
  for f in (a.sources/key).rglob('*'):
   if f.is_file() and f.suffix.lower()=='.txt' and f.name!='extraction-error.txt':
    target=dest/'original-credits'/key/f.relative_to(a.sources/key)
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,target)
 (dest/'README.md').write_text('\n'.join(notes)+'\n')
 (dest/'sources.json').write_text(json.dumps(provenance,indent=2)+'\n')
 shutil.copyfile(a.inventory or a.sources/'decoded/catalogue.json',dest/'source-inventory.json')
 gallery=a.output/'gallery';gallery.mkdir();cards=[]
 for r in rows:
  strip=Image.new('RGB',(960,180))
  for angle in range(3):
   path=a.captures/f'{r["car"]}_{r["mesh"]}_{angle}.bmp'
   with Image.open(path) as im:strip.paste(im.convert('RGB').resize((320,180)),(angle*320,0))
  strip.save(gallery/(r['id']+'.jpg'),quality=87)
  label=f'{r["car"]} · {r["pack"].upper()} · {r["label"]} · {r["id"]}'
  cards.append(f'<article><h2>{html.escape(label)}</h2><img loading="lazy" src="{r["id"]}.jpg" alt="{html.escape(label,quote=True)}"><p>{html.escape(r["texture"])}</p></article>')
 (gallery/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>OpenJuice vinyl preview</title>'
 '<style>body{font:16px system-ui;background:#151719;color:#eee;max-width:1100px;margin:30px auto;padding:20px}'
 'h2{font-size:16px}article{margin:30px 0;border-bottom:1px solid #555}img{width:100%;height:auto}'
 'input{font:inherit;padding:12px;width:90%;position:sticky;top:10px}</style>'
 '<h1>Underground vinyl collection</h1><p>Native garage captures. Stock body parts. '
 'Includes complete designs and independent hood, stripe and badge options.</p><input aria-label="Filter vinyls" placeholder="Filter by car, pack or design" '
 'oninput="for(const c of document.querySelectorAll(\'article\')) c.hidden=!c.textContent.toLowerCase().includes(this.value.toLowerCase())">'
 +''.join(cards))
 files=sorted(f for f in dest.rglob('*') if f.is_file())
 (dest/'SHA256SUMS').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(dest).as_posix()+'\n' for f in files))
 archive=a.output/('OpenJuice-Underground-Vinyls-'+a.version+'.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for f in sorted(dest.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(a.output))
 with zipfile.ZipFile(archive) as z:
  if z.testzip() is not None:raise RuntimeError('ZIP integrity failure')
 print(archive,archive.stat().st_size,'bytes')
 print(collections.Counter(r['pack'] for r in rows))

if __name__=='__main__':main()
