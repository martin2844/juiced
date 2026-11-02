"""Apply the approved405-only boost curve to a private r20 candidate."""
from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);a=p.parse_args()
path=a.candidate/'patches/scripts/dynamic_peugeot_405_glx.txt.patch';lines=path.read_text().splitlines();indices=[i for i,l in enumerate(lines) if l.startswith('text ')];assert len(indices)==1
i=indices[0];text=bytes.fromhex(lines[i][5:]).decode('latin1');assert 'TurboUpgradeScale' not in text
needle='MaxTorque 153\n';assert text.count(needle)==1
text=text.replace(needle,needle+'\n; Optional405-only built-engine / boost preparation by installed turbo tier.\n; Stock102BHP unchanged; full street/sports/competition/prototype 149/218/320/400BHP.\nTurboUpgradeScale 1.10 1.40 1.75 1.85409\n')
lines[i]='text '+text.encode('latin1').hex();path.write_text('\n'.join(lines)+'\n');print('Configured approved405-only preparation:',path)
hashes={p.relative_to(a.candidate).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in a.candidate.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(a.candidate/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
