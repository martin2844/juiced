"""Verify every exported AI receiver against its recorded native donor.

Only material identifiers may differ. Vertex/UV/damage bytes, indices,
declarations and bounds must survive unchanged in both garage and race.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'mod_bundle'))
from retail import Retail
from mesh import read, write


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--overlay', type=Path, required=True)
    ap.add_argument('--mod', type=Path, required=True)
    args = ap.parse_args()
    retail = Retail(args.game)
    rows = json.loads((args.mod / 'catalogue.json').read_text())
    seen = set()
    designs = {}
    meshes = 0
    for row in rows:
        car, model = row['car'], row['mesh']
        assert not row['reference'], 'Research controls must not ship'
        assert (car, model) not in seen, 'Duplicate car/design'
        seen.add((car, model))
        designs.setdefault(car, []).append(model)
        for source in row['sources']:
            path = args.overlay / source['path'].lower()
            raw = path.read_bytes() if path.is_file() else retail.read(source['path'])
            assert hashlib.sha256(raw).hexdigest() == source['sha256'], source['path']
            folder = car.lower() + ('_ui' if source['ui'] else '')
            output = args.mod / f'assets/cars/{folder}/mesh/{model.lower()}.d8m'
            original, generated = read(raw), read(output.read_bytes())
            assert len(original['parts']) == len(generated['parts'])
            for before, after in zip(original['parts'], generated['parts']):
                assert after['material'] == model, output
                after['header'][:32] = before['header'][:32]
            assert write(generated) == raw, f'Native geometry changed: {output}'
            meshes += 1
        for prefix, dimensions in [('cartextures/', (1024, 256)), ('ui/textures/ui_', (256, 64))]:
            data = (args.mod / f'assets/{prefix}{model.lower()}.ddt').read_bytes()
            fmt, width, height, levels = struct.unpack_from('<4I', data)
            assert fmt == 21 and (width, height) == dimensions and levels >= 1
            pixels = data[44:44 + width * height * 4]
            alpha = pixels[3::4]
            assert any(alpha) and min(alpha) == 0, 'Artwork needs visible pixels and transparent background'
    first = next(iter(designs.values()))
    assert all(models == first for models in designs.values()), 'Inconsistent design order across cars'
    print(json.dumps(dict(cars=len(designs), designs_per_car=len(first), combinations=len(rows),
                          exact_native_meshes=meshes, geometry_preserved=True), indent=2))


if __name__ == '__main__':
    main()
