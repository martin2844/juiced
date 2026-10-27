"""Build AI artwork on intact retail vinyl meshes, without small-decal fitting.

Output is isolated and disabled. Native geometry/UVs/colours are preserved.
Use --reference to include the unchanged retail material as a visual control.
"""
import argparse
import json
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
    ap.add_argument('--cars', default='CIVIC_97,350Z,SUPRA,RX7')
    ap.add_argument('--reference', action='store_true')
    a = ap.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    retail = Retail(a.game)
    inventory = json.loads(a.inventory.read_text())['rows']
    definitions = json.loads(a.definition.read_text())['designs']
    cars = {r['car'] for r in inventory} if a.cars == 'ALL' else set(a.cars.split(','))
    records, registry = [], []

    def put(path, data):
        dest = a.output/'assets'/path.lower()
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

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
            donors = [next(r for r in inventory if r['car'] == car and r['ui'] == ui
                           and r['style'] == design['donor']) for ui in (False,True)]
            variants = [(name, False)]
            if a.reference:
                variants.append((name+'_REF', True))
            for identifier, reference in variants:
                assert len(identifier) < 32
                for donor in donors:
                    mesh = read(retail.read(donor['path']))
                    for p, source in zip(mesh['parts'], donor['parts']):
                        if reference:
                            put(f'carmesh/mats/{identifier}.pmd', retail.read(source['material']))
                        p['header'][:32] = identifier.encode().ljust(32,b'\0')
                    put(f'cars/{car}{"_ui" if donor["ui"] else ""}/mesh/{identifier}.d8m', write(mesh))
                if reference:
                    put(f'ui/textures/ui_{identifier}.ddt', retail.read(donors[0]['thumbnail']))
                label = 'AI: '+design['name'] if not reference else 'RETAIL: '+design['donor']
                registry.append(f'Vinyl {car} {identifier} "{label}"\n')
                records.append(dict(car=car, mesh=identifier, donor=design['donor'],
                                    reference=reference, label=label))
    put('scripts/mod_vinyls.txt', ''.join(registry).encode())
    (a.output/'catalogue.json').write_text(json.dumps(records,indent=2)+'\n')
    (a.output/'mod.ini').write_text('name=AI Hero Liveries - preview\nversion=1\nrevision=1\nkind=assets\n')
    (a.output/'disabled').touch()
    print(len(records), 'car/design combinations; visual review required')


if __name__ == '__main__':
    main()
