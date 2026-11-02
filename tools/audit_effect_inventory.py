#!/usr/bin/env python3
"""Independently verify the test name inventory against all retail DAT payloads."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

from mod_bundle.retail import name_hash


def audit(game, manifest):
    names = re.findall(r'"(material_[a-z0-9_]+\.fxo)"', manifest.read_text())
    lookup = {name_hash(name): name for name in names}
    if len(names) != 74 or len(lookup) != 74:
        raise ValueError('Expected 74 distinct names and hashes')
    archive = (game / 'effects.dat').read_bytes()
    count, table = struct.unpack_from('<II', archive)
    entries = list(struct.iter_unpack('<5I', archive[table:table + count * 20]))
    if len(entries) != count or len({e[0] for e in entries}) != count:
        raise ValueError('Incomplete or duplicate archive directory')
    found = {}
    for key, offset, chunks, _packed_size, size in entries:
        chunks_data = archive[offset:offset + chunks * 8]
        if len(chunks_data) != chunks * 8:
            raise ValueError('Truncated chunk directory')
        payload = bytearray()
        for pos, length in struct.iter_unpack('<II', chunks_data):
            compressed = archive[pos:pos + length]
            if len(compressed) != length:
                raise ValueError('Truncated chunk')
            payload.extend(zlib.decompress(compressed))
        if len(payload) != size:
            raise ValueError('Inflated size mismatch')
        # Classify by binary signature, independently of candidate names.
        if payload[:4] == bytes.fromhex('0109fffe'):
            if key not in lookup:
                raise ValueError(f'FXO entry missing from manifest: {key}')
            found[lookup[key]] = hashlib.sha256(payload).hexdigest()
    if set(found) != set(names):
        raise ValueError('Manifest names missing from FXO archive entries')
    return {'archive_sha256': hashlib.sha256(archive).hexdigest(),
            'manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
            'archive_entries': count, 'effects': found}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--manifest', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tests/Framework/RetailEffects.h')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.game, args.manifest)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Verified {len(report['effects'])} FXO entries in {report['archive_entries']} archive entries")
