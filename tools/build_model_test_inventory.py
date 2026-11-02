#!/usr/bin/env python3
"""Resolve externally supplied filenames against retail DATs into a test fixture.

Does not extract assets or change the game installation. Unknown hashes are
reported, not treated as covered. The fixture uses symlinks to original assets.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct

from mod_bundle.retail import name_hash


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--names', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    args = parser.parse_args()
    game = args.game.resolve()
    if args.fixture.exists():
        raise ValueError('Use a new fixture directory')
    lookup = defaultdict(set)
    for line in args.names.read_text().splitlines():
        name = line.strip().replace('\\', '/').lower()
        if not name:
            continue
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError('Unsafe candidate filename')
        lookup[name_hash(name)].add(name)
    paths, unknown, ambiguous = set(), [], []
    archives = {}
    for archive in sorted(game.rglob('*.dat')):
        relative = archive.relative_to(game)
        if 'audio' in relative.parts or 'attracts' in relative.parts:
            continue  # different container formats
        with archive.open('rb') as stream:
            header = stream.read(8)
            if len(header) != 8:
                continue
            count, table = struct.unpack('<II', header)
            if table + count * 20 > archive.stat().st_size:
                continue
            stream.seek(table)
            directory = stream.read(count * 20)
        archives[str(relative)] = hashlib.sha256(directory).hexdigest()
        for key, *_ in struct.iter_unpack('<5I', directory):
            names = lookup.get(key, set())
            record = {'archive': str(relative), 'hash': key}
            if not names:
                unknown.append(record)
            elif len(names) > 1:
                ambiguous.append(dict(record, names=sorted(names)))
            else:
                name = next(iter(names))
                if Path(name).suffix in ('.boh', '.d8m', '.pmd'):
                    paths.add(str(relative.with_suffix('') / name))
    args.fixture.mkdir(parents=True)
    for child in game.iterdir():
        (args.fixture / child.name).symlink_to(child, target_is_directory=child.is_dir())
    (args.fixture / '.oj-model-inventory.txt').write_text('\n'.join(sorted(paths)) + '\n')
    report = dict(models=dict(Counter(Path(p).suffix for p in paths)),
                  names_sha256=hashlib.sha256(args.names.read_bytes()).hexdigest(),
                  archive_directory_sha256=archives, unknown=unknown, ambiguous=ambiguous)
    args.fixture.with_suffix('.report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['models'], 'unknown', len(unknown), 'ambiguous', len(ambiguous))


if __name__ == '__main__':
    main()
