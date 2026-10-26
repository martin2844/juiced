"""Decode source DDS vinyls, preserve provenance and report every excluded file."""
from pathlib import Path
from PIL import Image
import numpy as np
import re,json,hashlib,argparse
ALIASES={
 'SKYLINE':['skyline','r34','r-34','gtrs','gtr','r-atr','skyatr','sky-atr','penzoil','pennzoil'],
 'SUPRA':['supra','sup','fafsup','fnfsup'],
 'RX7':['rx7','rx-7','rx_7'],
 'RX8':['rx8','rx-8'],
 'ECLIPSE':['eclipse'],
 'S2000':['s2000','s2k','honda s2000','drago'],
 'EVO8':['lancerevo','evo','lancer'],
 '350Z':['350z','350 z','z350','z-350','350_z'],
 'CIVIC_97':['civic','civik'],
 'PEUGEOT_206':['peugeot','peugot','peug','206'],
 'MX5':['miata','mx5','mx-5'],
 'INTEGRA':['integra'],
 'RSX':['rsx'],
 'CELICA':['celica'],
 'FOCUS':['focus','mcrae'],
 'GOLF':['golf','vwgolf','oettinger'],
 'WRX2':['impreza','subaru','wrx'],
 'NEON':['neon'],
 'COROLLA':['corolla','trueno','ae86','fujiwara'],
 'MUSTANG2':['mustang'],
}
def canonical(s):
 return re.sub(r'[^a-z0-9]','',s.lower())
def load_dds(p):
 # DDS often has harmless trailing export metadata; Pillow reads its declared image.
 return Image.open(p).convert('RGBA')
def build(root,out):
 out.mkdir(parents=True,exist_ok=True)
 rows=[];excluded=[];seen={}
 for pack in ['nd4','fnf','aj']:
  paths=sorted((root/pack).rglob('*'))
  files=[p for p in paths if p.suffix.lower()=='.dds']
  for p in files:
   stem=p.stem.lower()
   if re.search(r'(mask|_mas|_m$|mask\))',stem): continue
   # Exporters also use <name>m.dds / <name>M.dds for masks.
   if stem.endswith('m') and any(q.stem.lower()==stem[:-1] for q in files if q.parent==p.parent):continue
   if any(k in str(p).lower() for k in ['texture_oz','brakelight','headlight','newneonlight','_wheel','missingparts.zip','/texture/']):
    excluded.append(dict(path=str(p.relative_to(root)),reason='Non-vinyl texture (wheel/light/body)'));continue
   rel=p.relative_to(root).as_posix()
   if '2f2fskylinev2.zip.contents/' in rel and p.name=='2.dds':
    excluded.append(dict(path=rel,reason='Numeric companion opacity mask for 1.dds'));continue
   if stem.endswith('_') and any(q.stem.lower()==stem[:-1] for q in files if q.parent==p.parent):
    excluded.append(dict(path=rel,reason='Companion mask (trailing underscore naming)'));continue
   candidates=[q for q in files if q.parent==p.parent and q!=p and (
       q.stem.lower() in [stem+'_mask',stem+'_mas',stem+'_m',stem+'m',stem+'mask',stem+'_'] or
       ('mask' in q.stem.lower() and canonical(q.stem.replace('_mask',''))==canonical(p.stem)))]
   if not candidates:
    candidates=[q for q in files if q.parent==p.parent and 'mask' in q.stem.lower() and
        canonical(q.stem).startswith(canonical(p.stem.split('(')[0]))]
   if '2f2fskylinev2.zip.contents/' in rel and p.name=='1.dds':candidates=[p.with_name('2.dds')]
   try:
    im=load_dds(p);a=np.array(im)
    tint_channels=False
    if candidates:
     mask=load_dds(candidates[0]).resize(im.size)
     maskarr=np.array(mask)
     # Mask RGB defines the footprint; source alpha may be an opaque colour layer.
     footprint=maskarr[:,:,:3].max(2)
     rgb=a[:,:,:3].astype(float)
     colourful=(rgb.max(2)-rgb.min(2)>64)&(footprint>64)
     primary=(rgb.max(2)>100)&(np.sort(rgb,axis=2)[:,:,1]<32)
     tint_channels=bool(colourful.any() and primary[colourful].mean()>.98)
     source_visible=a[:,:,3]>64
     overlap=((footprint>64)&source_visible).sum()/max(int(source_visible.sum()),1)
     # Tint maps use alpha as a colour channel; full-colour artwork can instead
     # use it to cut a graphic out of a broad rectangular mask (e.g. AJ_Lethal).
     if not tint_channels and source_visible.any() and overlap>.8:
      footprint=np.minimum(footprint,a[:,:,3])
     a[:,:,3]=footprint
    # Do not invent missing colour palettes. These colour-channel assets are marked.
    opaque=a[a[:,:,3]>128,:3]
    key=hashlib.sha256(a.tobytes()+str(im.size).encode()).hexdigest()
    if key in seen:
     seen[key]['also_from'].append(rel);continue
    source=p.relative_to(root/pack).parts[0].lower()
    label=re.sub(r'\.(zip|rar)\.contents$','',source)
    context=(str(p.relative_to(root/pack))+' '+label).lower()
    # Prefer the texture's actual car name over incidental names in the archive.
    model=next((car for car,words in ALIASES.items() if any(w in p.stem.lower() for w in words)),None)
    if not model:model=next((car for car,words in ALIASES.items() if any(w in context for w in words)),None)
    if not model:
     for txt in p.parent.glob('*.txt'):
      context+=' '+txt.read_text(errors='replace').lower()
     model=next((car for car,words in ALIASES.items() if any(w in context for w in words)),None)
    adaptation=None
    if '240sx' in context:model='300ZX';adaptation='240SX artwork retargeted to the available Nissan 300ZX'
    elif 'tiburon' in context:model='CELICA';adaptation='Tiburon artwork retargeted to the available Toyota Celica'
    elif not model:model='GOLF';adaptation='Unspecified source car: adapted to the Golf'
    identifier='V'+key[:10].upper()
    target=out/(identifier+'.png');Image.fromarray(a).save(target)
    row=dict(id=identifier,pack=pack,path=rel,label=label,texture=p.name,model=model,
             width=im.width,height=im.height,mask=str(candidates[0].relative_to(root)) if candidates else None,
             adaptation=adaptation,layout='badge' if im.width!=im.height else 'atlas',tint_channels=tint_channels,
             sha256=hashlib.sha256(p.read_bytes()).hexdigest(),also_from=[],png=target.name)
    rows.append(row);seen[key]=row
   except Exception as e:excluded.append(dict(path=rel,reason=str(e)))
 (out/'catalogue.json').write_text(json.dumps(dict(designs=rows,excluded=excluded),indent=2))
 print('designs',len(rows),'excluded',len(excluded),'unmapped',sum(r['model'] is None for r in rows))
 for r in rows:
  if not r['model']: print('UNMAPPED',r['path'])
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
 a=p.parse_args();build(a.root,a.output)
