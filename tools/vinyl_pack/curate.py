"""Copy only visually accepted vinyls into a fresh package, preserving source data."""
import argparse
import collections
import json
import shutil
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--review', type=Path, default=Path(__file__).with_name('visual_tiers_v2.json'))
    a = p.parse_args()
    rows = json.loads((a.source / 'catalogue.json').read_text())
    review = json.loads(a.review.read_text())
    decisions = {r['id']: r for r in review['entries']}
    assert len(decisions) == len(review['entries'])
    assert set(decisions) == {r['id'] for r in rows}, 'Review must cover source exactly'
    keep = [r for r in rows if decisions[r['id']]['tier'] in ('A', 'B')]
    ids = {r['id'] for r in keep}
    registry = (a.source / 'assets/scripts/mod_vinyls.txt').read_text().splitlines()
    lines = [line for line in registry if line.split()[2].removeprefix('DECAL_NFS_') in ids]
    assert [line.split()[2] for line in lines] == [r['mesh'] for r in keep]
    files = []
    for r in keep:
        assert r['mesh'] == 'DECAL_NFS_' + r['id']
        car, mesh = r['car'].lower(), r['mesh'].lower()
        files.extend([
            f'cars/{car}/mesh/{mesh}.d8m',
            f'cars/{car}_ui/mesh/{mesh}.d8m',
            f'cartextures/{mesh}.ddt',
            f'carmesh/mats/{mesh}.pmd',
            f'ui/textures/ui_{mesh}.ddt',
        ])
    assert all((a.source / 'assets' / f).is_file() for f in files)
    a.output.mkdir(parents=True, exist_ok=False)
    for f in files:
        target = a.output / 'assets' / f
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(a.source / 'assets' / f, target)
    script = a.output / 'assets/scripts/mod_vinyls.txt'
    script.parent.mkdir(parents=True)
    script.write_text('\n'.join(lines) + '\n')
    (a.output / 'catalogue.json').write_text(json.dumps(keep, indent=2) + '\n')
    (a.output / 'mod.ini').write_text('name=Underground & F&F Vinyl Collection - Curated A+B\nversion=1\nrevision=3\nkind=assets\n')
    selection = dict(source=review['collection'], tiers=['A', 'B'],
                     counts=dict(collections.Counter(decisions[r['id']]['tier'] for r in keep)),
                     kept=[r['id'] for r in keep], removed=[r['id'] for r in rows if r['id'] not in ids])
    (a.output / 'selection.json').write_text(json.dumps(selection, indent=2) + '\n')
    shutil.copyfile(a.review, a.output / 'visual-review.json')
    actual = {str(f.relative_to(a.output / 'assets')) for f in (a.output / 'assets').rglob('*') if f.is_file()}
    assert actual == set(files) | {'scripts/mod_vinyls.txt'}, 'Unexpected or missing assets'
    print(f'Kept {len(keep)}, removed {len(rows)-len(keep)}; verified {len(actual)} asset files')


if __name__ == '__main__':
    main()
