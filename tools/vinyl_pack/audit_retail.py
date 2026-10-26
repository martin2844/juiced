"""Inventory retail vinyl resources read-only, following CarModel resource lookup.

Does not launch the game, install assets, modify saves, or certify visual quality.
Requires NumPy (the native D8M reader). Reports missing resources explicitly.
"""
import argparse
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'mod_bundle'))
from retail import Retail
from mesh import read


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--package', type=Path, help='Optional existing imported package to compare')
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    retail = Retail(args.game)
    resources = {}
    errors = []
    rows = []

    def resolve(paths):
        for path in paths:
            try:
                raw = retail.read(path)
                resources.setdefault(path, dict(sha256=digest(raw), bytes=len(raw)))
                return path, raw
            except FileNotFoundError:
                pass
        raise FileNotFoundError(' | '.join(paths))

    def texture(paths):
        path, raw = resolve(paths)
        resources[path]['header'] = list(struct.unpack_from('<11I', raw))
        return path

    _, cardefs = resolve(['scripts/cardefs.txt'])
    blocks = re.findall(r'\[BeginCar\]\s+(\S+)(.*?)\[EndCar\]', cardefs.decode('latin1'), re.S | re.I)
    for car, block in blocks:
        names = re.findall(r'^\s*CUSTOM\s+DECAL\s+(\S+)', block, re.M | re.I)
        for index, name in enumerate(names):
            style = name[name.upper().index('DECAL'):].lower()
            for ui in (False, True):
                folder = f'cars/{car.lower()}{"_ui" if ui else ""}'
                row = dict(car=car, index=index, style=style, ui=ui, parts=[])
                rows.append(row)
                try:
                    row['thumbnail'] = texture([f'ui/textures/ui_{style}.ddt'])
                    path, raw = resolve([f'{folder}/mesh/{name.lower()}.d8m', f'carmesh/{name.lower()}.d8m', f'mesh/{name.lower()}.d8m'])
                    row.update(path=path, sha256=digest(raw))
                    mesh = read(raw)
                    for part in mesh['parts']:
                        mat = part['material'].lower()
                        matpath, matraw = resolve([f'{folder}/mesh/mats/{mat}.pmd', f'carmesh/mats/{mat}.pmd', f'mesh/mats/{mat}.pmd'])
                        text = matraw.decode('latin1')
                        resources[matpath]['text'] = text
                        textures = []
                        for tex in re.findall(r'Texture\s*=\s*([^\s]+)', text, re.I):
                            tex = tex.replace('\\', '/').split('/')[-1].lower().removesuffix('.ptd')
                            textures.append(texture([f'{folder}/textures/{tex}.ddt', f'cartextures/{tex}.ddt']))
                        pos = part['streams'][(0, 0)]
                        uv = part['streams'].get((5, 0))
                        row['parts'].append(dict(material=matpath, textures=textures, vertices=len(pos),
                            position_min=pos.min(0).tolist(), position_max=pos.max(0).tolist(),
                            geometry_hash=digest(pos.tobytes()), uv_hash=None if uv is None else digest(uv.tobytes()),
                            uv_min=None if uv is None else uv.min(0).tolist(), uv_max=None if uv is None else uv.max(0).tolist()))
                except (FileNotFoundError, ValueError, AssertionError, struct.error) as exc:
                    errors.append(dict(car=car, style=style, ui=ui, error=str(exc)))
    groups = collections.defaultdict(list)
    for row in rows:
        groups[(row['car'], row['ui'])].append(row)
    summary = dict(car_definition_blocks=len(blocks), cars_with_decals=len({r['car'] for r in rows}),
        blocks_without_decals=[car for car, _ in blocks if car not in {r['car'] for r in rows}], styles=len({r['style'] for r in rows}), mesh_instances=len(rows),
        lookup_errors=len(errors), resources=len(resources),
        styles_per_car_variant=dict(collections.Counter(len(v) for v in groups.values())),
        unique_geometry_per_car_variant=dict(collections.Counter(
            len({tuple(p['geometry_hash'] for p in r['parts']) for r in v}) for v in groups.values())),
        thumbnail_dimensions=dict(collections.Counter(
            f"{resources[p]['header'][1]}x{resources[p]['header'][2]}" for p in {r['thumbnail'] for r in rows if 'thumbnail' in r})))
    comparison = None
    if args.package:
        imported = json.loads((args.package / 'catalogue.json').read_text())
        dimensions = collections.Counter()
        for item in imported:
            raw = (args.package / 'assets/ui/textures' / ('ui_' + item['mesh'].lower() + '.ddt')).read_bytes()
            _, width, height = struct.unpack_from('<3I', raw)
            dimensions[f'{width}x{height}'] += 1
        comparison = dict(package=str(args.package.resolve()), options=len(imported),
            options_per_car=dict(collections.Counter(r['car'] for r in imported)),
            thumbnail_dimensions=dict(dimensions),
            current_import_capacity_after_31_retail=125-31,
            total_if_all_options_added_per_car_with_logo=31+len(imported)+1)
    report = dict(game=str(args.game.resolve()), summary=summary, comparison=comparison,
                  rows=rows, resources=resources, errors=errors)
    (args.output / 'retail-inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))
    if comparison:
        (args.output / 'comparison.json').write_text(json.dumps(comparison, indent=2) + '\n')
        print(json.dumps(comparison, indent=2))
    if errors:
        print(json.dumps(errors[:10], indent=2))
        raise SystemExit(1)


if __name__ == '__main__':
    main()
