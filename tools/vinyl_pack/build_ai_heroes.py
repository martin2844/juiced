"""Build AI artwork on intact retail vinyl meshes, without small-decal fitting.

Output is isolated and disabled. Native geometry/UVs/colours are preserved.
Use --reference to include the unchanged retail material as a visual control.
"""
import argparse
import json
import hashlib
import re
from pathlib import Path
from PIL import Image
from build import ddt
from mesh import read, write
from retail import Retail


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--definition', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--cars', default='ALL')
    ap.add_argument('--overlay', type=Path, help='Assembled active mods: include their real per-car vinyl meshes and catalogue')
    ap.add_argument('--reference', action='store_true')
    a = ap.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    retail = Retail(a.game)
    inventory = json.loads(a.inventory.read_text())['rows']
    definitions = json.loads(a.definition.read_text())['designs']
    retail_cars = {r['car'].casefold(): r['car'] for r in inventory}
    if a.overlay:
        text = (a.overlay/'scripts/cardefs.txt').read_text(encoding='latin1')
        blocks = re.findall(r'^[ \t]*\[BeginCar\]\s+(\S+)(.*?)^[ \t]*\[EndCar\]', text, re.I | re.M | re.S)
        roster = {name: re.findall(r'^[ \t]*CUSTOM\s+DECAL\s+"?([A-Za-z0-9_]+)', block, re.M | re.I)
                  for name, block in blocks if name.casefold() != 'none'}
    else:
        roster = {name: [] for name in retail_cars.values()}
    requested = None if a.cars == 'ALL' else {c.casefold() for c in a.cars.split(',')}
    cars = {name for name in roster if requested is None or name.casefold() in requested}
    if requested is not None and {name.casefold() for name in cars} != requested:
        raise ValueError('Requested cars are absent from the assembled catalogue')
    if not cars:
        raise ValueError('No cars in catalogue')
    records, registry = [], []

    def put(path, data):
        dest = a.output/'assets'/path.lower()
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    def resource(path):
        if a.overlay and (a.overlay/path.lower()).is_file():
            return (a.overlay/path.lower()).read_bytes()
        return retail.read(path)

    def donor_for(car, style, ui):
        rows = [r for r in inventory if r['car'].casefold() == car.casefold()
                and r['ui'] == ui and r['style'].casefold() == style.casefold()]
        if rows:
            row = rows[0]
            return row['path'], resource(row['path']), row.get('thumbnail')
        # Added cars already carry fitted native vinyl receivers. Reuse their
        # exact mesh/UV/damage streams, not another car's side or a rectangle.
        models = [name for name in roster[car] if name.casefold().endswith('_'+style.casefold())
                  or name.casefold() == style.casefold()]
        if len(models) != 1:
            raise ValueError(f'{car}: expected one fitted receiver for {style}, found {models}')
        folder = f'cars/{car.lower()}{"_ui" if ui else ""}'
        path = f'{folder}/mesh/{models[0].lower()}.d8m'
        try:
            return path, resource(path), f'ui/textures/ui_{style.lower()}.ddt'
        except FileNotFoundError as exc:
            raise ValueError(f'{car}: missing fitted vinyl receiver {path}') from exc

    for design in definitions:
        image = Image.open(a.definition.parent/design['art']).convert('RGBA')
        # Image generation can leave almost transparent pixels well outside
        # the artwork. Do not let that noise shrink the visible livery.
        box = image.getchannel('A').point(lambda v: 255 if v > 8 else 0).getbbox()
        if not box:
            raise ValueError('Empty AI image')
        # Texture compilation: normalize the generated strip to the retail
        # 4:1 canvas. No changes to car geometry or UV coordinates.
        image = image.crop(box).resize((1024, 256), Image.Resampling.LANCZOS)
        name = design['mesh']
        put(f'cartextures/{name}.ddt', ddt(image, True))
        put(f'ui/textures/ui_{name}.ddt', ddt(image.resize((256,64), Image.Resampling.LANCZOS)))
        put(f'carmesh/mats/{name}.pmd', (
            'Material Program = Standard Alpha Blend\nZ-Write = FALSE\n'
            f'Disable Fog = FALSE\nAlpha Texture = {name}.ptd\n').encode())
        for car in sorted(cars):
            donors = [donor_for(car, design['donor'], ui) for ui in (False,True)]
            variants = [(name, False)]
            if a.reference:
                variants.append((name+'_REF', True))
            for identifier, reference in variants:
                assert len(identifier) < 32
                donor_hashes = []
                for ui, (path, raw, _) in zip((False, True), donors):
                    mesh = read(raw)
                    donor_hashes.append(dict(ui=ui, path=path, sha256=hashlib.sha256(raw).hexdigest()))
                    for p in mesh['parts']:
                        if reference:
                            material = p['material'].lower()
                            paths = [f'cars/{car.lower()}{"_ui" if ui else ""}/mesh/mats/{material}.pmd',
                                     f'carmesh/mats/{material}.pmd', f'mesh/mats/{material}.pmd']
                            for source in paths:
                                try:
                                    put(f'carmesh/mats/{identifier}.pmd', resource(source));break
                                except FileNotFoundError:
                                    pass
                            else:
                                raise ValueError(f'Missing reference material {material}')
                        p['header'][:32] = identifier.encode().ljust(32,b'\0')
                    put(f'cars/{car}{"_ui" if ui else ""}/mesh/{identifier}.d8m', write(mesh))
                if reference:
                    put(f'ui/textures/ui_{identifier}.ddt', resource(donors[0][2]))
                label = 'AI: '+design['name'] if not reference else 'RETAIL: '+design['donor']
                optional = car.casefold() not in retail_cars
                registry.append(f'{"VinylIfPresent" if optional else "Vinyl"} {car} {identifier} "{label}"\n')
                records.append(dict(car=car, mesh=identifier, donor=design['donor'],
                                    reference=reference, label=label, optional_car=optional,
                                    sources=donor_hashes))
    put('scripts/mod_vinyls.txt', ''.join(registry).encode())
    (a.output/'catalogue.json').write_text(json.dumps(records,indent=2)+'\n')
    (a.output/'mod.ini').write_text('name=AI Hero Liveries - all installed cars\nversion=1\nrevision=2\nkind=assets\n')
    (a.output/'disabled').touch()
    print(len(records), 'car/design combinations; visual review required')


if __name__ == '__main__':
    main()
