"""Inventory downloaded vinyl archives. Never execute bundled installers."""
import argparse, hashlib, json, subprocess
from pathlib import Path

def unpack(root, tar, unrar=None):
    for depth in range(5):
        todo=[p for p in root.rglob('*') if p.suffix.lower() in ('.zip','.rar','.7z') and not p.with_name(p.name+'.contents').exists()]
        if not todo: break
        for p in todo:
            target=p.with_name(p.name+'.contents');target.mkdir()
            command=([unrar,'x','-o+','-idq',str(p.resolve()),str(target.resolve())+'/']
                     if unrar and p.suffix.lower()=='.rar' else
                     [tar,'-xf',str(p.resolve()),'-C',str(target.resolve())])
            result=subprocess.run(command,capture_output=True)
            if result.returncode: (target/'extraction-error.txt').write_bytes(result.stderr)
    rows=[]
    for p in sorted(root.rglob('*')):
        if p.is_file() and p.suffix.lower() not in ('.zip','.rar','.7z'):
            rows.append(dict(path=str(p.relative_to(root)),size=p.stat().st_size,
                             sha256=hashlib.sha256(p.read_bytes()).hexdigest(),extension=p.suffix.lower()))
    return rows

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('root',type=Path);a.add_argument('--tar',default='bsdtar')
    a.add_argument('--unrar',help='Preferred extractor for the older RAR archives')
    args=a.parse_args()
    rows=unpack(args.root,args.tar,args.unrar)
    (args.root/'inventory.json').write_text(json.dumps(rows,indent=2))
    from collections import Counter
    print(Counter(r['extension'] for r in rows))
