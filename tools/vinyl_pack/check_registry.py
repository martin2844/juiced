"""Negative checks against the real native catalogue loader; no production saves."""
import argparse
import subprocess
import tempfile
from pathlib import Path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('binary',type=Path)
    parser.add_argument('game',type=Path)
    args=parser.parse_args()
    binary=args.binary.resolve();game=args.game.resolve()
    cases={
        'absent':(None,'No imported vinyls'),
        'unknown car':('Vinyl NONEXISTENT DECAL_NFS_V000 "test"','Invalid car or mesh'),
        'path traversal':('Vinyl SUPRA ../DECAL_NFS_V000 "test"','Invalid car or mesh'),
        'duplicate':(('Vinyl SUPRA DECAL_NFS_V000 "test"\n')*2,'Duplicate vinyl'),
        'overflow':('\n'.join(f'Vinyl SUPRA DECAL_NFS_V{i:05} "test"' for i in range(126)),
                    'decal capacity exceeded'),
    }
    for name,(script,message) in cases.items():
        with tempfile.TemporaryDirectory(prefix='openjuice-vinyl-test-') as temp:
            root=Path(temp);(root/'scripts').mkdir()
            if script is not None:(root/'scripts/mod_vinyls.txt').write_text(script)
            result=subprocess.run([str(binary),str(game),str(root)],capture_output=True,text=True)
            if result.returncode!=1 or message not in result.stderr+result.stdout:
                raise RuntimeError(f'{name}: unexpected native result: {result.stderr[-1000:]}')
        print('PASS',name)

if __name__=='__main__':main()
