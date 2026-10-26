"""Stage reviewed universal side motifs, with provenance and a real-render gallery.

The staged package must pass android_vinyl_check before distribution. This does
not confer approval on complete source liveries, body kits or Android hardware.
"""
import argparse
import hashlib
import html
import json
import shutil
from pathlib import Path
from PIL import Image
from package import SOURCES


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('candidate', 'selection', 'captures', 'sources', 'output'):
        ap.add_argument('--' + name, type=Path, required=True)
    a = ap.parse_args()
    selected = set(json.loads(a.selection.read_text())['selected_ids'])
    rows = [r for r in json.loads((a.candidate/'catalogue.json').read_text())
            if r['source_id'] in selected]
    cars = {r['car'] for r in rows}
    assert len(cars) == 52 and len(rows) == len(selected)*52
    assert all({r['source_id'] for r in rows if r['car'] == car} == selected for car in cars)
    assert len(selected) <= 94
    a.output.mkdir(parents=True, exist_ok=False)
    dest = a.output/'universal-side-motifs'
    assets = dest/'assets'
    meshes = {r['mesh'].lower() for r in rows}
    for source in (a.candidate/'assets').rglob('*'):
        if not source.is_file() or source.stem.removeprefix('ui_') not in meshes:
            continue
        target = assets/source.relative_to(a.candidate/'assets')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    registry = assets/'scripts/mod_vinyls.txt'
    registry.parent.mkdir(parents=True)
    registry.write_text(''.join(f'Vinyl {r["car"]} {r["mesh"]} "{r["label"]}"\n' for r in rows))
    (dest/'mod.ini').write_text('name=Universal Side Motifs\nversion=1\nrevision=1\nkind=assets\n')
    (dest/'disabled').touch()
    for r in rows:
        r.pop('accepted', None)
        r['review_scope'] = 'stock body; Civic art review and all-car placement sample'
    (dest/'catalogue.json').write_text(json.dumps(rows, indent=2)+'\n')
    for name in ('body-frames.json', 'placements.json'):
        shutil.copyfile(a.candidate/name, dest/name)
    shutil.copyfile(a.selection, dest/'selection.json')
    provenance = []
    for key, (credit, url, archive) in SOURCES.items():
        provenance.append(dict(pack=key, credit=credit, url=url,
            archive_sha256=hashlib.sha256((a.sources/archive).read_bytes()).hexdigest()))
        for f in (a.sources/key).rglob('*.txt'):
            if f.name == 'extraction-error.txt':
                continue
            target = dest/'original-credits'/key/f.relative_to(a.sources/key)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f, target)
    (dest/'sources.json').write_text(json.dumps(provenance, indent=2)+'\n')
    notes = f'''# Universal Side Motifs — edition 1

{len(selected)} standalone side motifs, available in the same order on all 52 original cars.
These are extracted lateral artwork, not complete NFS/F&F liveries.
Each has a transparent 256×64 workshop icon, shared texture and per-car geometry.
Artwork retains fixed source colours; no recolouring support is claimed.

Import through the OpenJuice Mods launcher. Disable the old Underground/F&F
vinyl pack first: both provide scripts/mod_vinyls.txt and cannot be combined.
This edition starts disabled. Requires the imported-vinyl catalogue extension
in the OpenJuice port (Android .10 or newer).

No saves are included, rewritten or partitioned. Changing the catalogue can
change a previously fitted vinyl because the game stores its numeric index.

Validation scope: stock bodywork. All combinations must pass garage/race mesh
loading and isolated save round trips. Both sides of every selected motif were
reviewed on the Civic; a text motif was used to calibrate placement across all
52 stock cars. This is not a visual review of every car/design combination.
Body kits, crash deformation, mod-added cars and this edition on Android
hardware remain unverified. See validation.txt for the actual test result.

Original source attribution is in sources.json and original-credits/.
OpenJuice upstream: https://gitlab.com/sidecr3w/openjuice
'''
    (dest/'README.md').write_text(notes)
    gallery = a.output/'gallery'
    gallery.mkdir()
    cards = []
    for r in rows:
        if r['car'] != 'CIVIC_97':
            continue
        strip = Image.new('RGB', (1280, 720))
        for angle in range(2):
            f = a.captures/f'{r["car"]}_{r["mesh"]}_{angle}.bmp'
            with Image.open(f) as im:
                strip.paste(im.convert('RGB').resize((640, 360)), (angle*640, 0))
        motif = Image.open(a.candidate/(r['mesh']+'.png')).convert('RGBA')
        strip.paste((55,55,55), (0,360,1280,720))
        strip.paste(motif, (128,400), motif)
        strip.save(gallery/(r['source_id']+'.jpg'), quality=92)
        cards.append(f'<article><h2>{html.escape(r["label"])} · {r["source_id"]}</h2>'
                     f'<img loading="lazy" src="{r["source_id"]}.jpg"></article>')
    (gallery/'index.html').write_text('<!doctype html><meta charset="utf-8">'
        '<title>Universal Side Motifs</title><style>body{background:#181818;color:#eee;'
        'font:16px system-ui;max-width:1280px;margin:30px auto}img{width:100%}</style>'
        '<h1>Universal Side Motifs</h1><p>Actual Civic renders, both sides, plus source motif. '
        'Stock bodywork; lateral decals, not full-car wraps.</p>'+''.join(cards))
    print(dest, len(selected), 'designs;', len(rows), 'combinations; TEST BEFORE ZIP')


if __name__ == '__main__':
    main()
