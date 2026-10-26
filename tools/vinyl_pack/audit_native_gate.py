"""Reject legacy conversions that fail the native-first prerequisites.

This is a structural review, not a visual certificate. Passing structural checks
only makes an option eligible for new multi-car visual/runtime review.
"""
import argparse
import collections
import hashlib
import json
import shlex
import struct
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(package, retail, previous, published):
    rows = json.loads((package / 'catalogue.json').read_text())
    native = json.loads(retail.read_text())
    assert not native['errors'], 'Retail inventory must be complete'
    cars = {r['car'].upper() for r in native['rows']}
    assert cars, 'No target cars'
    ratings = {r['id']: r['tier'] for r in json.loads(previous.read_text())['entries']}
    installed = {r['id'] for r in json.loads(published.read_text())}
    registry = collections.defaultdict(set)
    for line in (package / 'assets/scripts/mod_vinyls.txt').read_text().splitlines():
        words = shlex.split(line)
        if words and words[0] == 'Vinyl':
            registry[words[2]].add(words[1].upper())
    entries = []
    for row in rows:
        mesh = row['mesh']
        registered = registry[mesh]
        ready = []
        for car in sorted(cars):
            paths = [package / 'assets/cars' / (car.lower() + suffix) / 'mesh' / (mesh.lower() + '.d8m')
                     for suffix in ('', '_ui')]
            if car in registered and all(p.is_file() for p in paths):
                ready.append(car)
        thumb = package / 'assets/ui/textures' / ('ui_' + mesh.lower() + '.ddt')
        dims = None
        if thumb.is_file():
            raw = thumb.read_bytes()
            if len(raw) >= 44:
                _, w, h = struct.unpack_from('<3I', raw)
                dims = [w, h]
        failures = []
        if set(ready) != cars:
            failures.append('missing_registered_garage_and_race_variants_for_all_retail_cars')
        if dims != [256, 64]:
            failures.append('thumbnail_does_not_match_native_256x64_contract')
        # No legacy A/B rating constitutes the new visual/runtime acceptance.
        failures.append('native_first_visual_and_runtime_acceptance_missing')
        entries.append(dict(id=row['id'], label=row['label'], source_car=row['car'], pack=row['pack'],
                            previous_tier=ratings.get(row['id']), in_published_107=row['id'] in installed,
                            registered_cars=sorted(registered), complete_car_variants=ready,
                            required_car_count=len(cars), thumbnail_dimensions=dims,
                            decision='exclude', reasons=failures))
    assert len({r['id'] for r in entries}) == len(rows), 'Duplicate IDs'
    return dict(schema=1, policy='native-first-2026-10-26',
                scope='Every option in the complete legacy conversion, including previously excluded C/D.',
                method='Structural eligibility checks; no new manual visual verdict is inferred.',
                source_catalogue_sha256=sha(package / 'catalogue.json'),
                source_registry_sha256=sha(package / 'assets/scripts/mod_vinyls.txt'),
                retail_inventory_sha256=sha(retail),
                summary=dict(reviewed=len(entries), accepted=0, excluded=len(entries),
                             published_reviewed=sum(e['in_published_107'] for e in entries),
                             required_cars=len(cars),
                             reasons=dict(collections.Counter(reason for e in entries for reason in e['reasons']))),
                accepted_ids=[], entries=entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--retail', type=Path, required=True)
    parser.add_argument('--previous-review', type=Path, default=Path(__file__).with_name('visual_tiers_v2.json'))
    parser.add_argument('--published-catalogue', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.package, args.retail, args.previous_review, args.published_catalogue)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report['summary'], indent=2))


if __name__ == '__main__':
    main()
