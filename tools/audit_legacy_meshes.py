#!/usr/bin/env python3
"""Classify rejected D8Ms structurally; does not establish runtime reachability.

Usage: PYTHONPATH=tools python tools/audit_legacy_meshes.py GAME FAILURES_JSON OUTPUT
The candidate old layouts have bounds, shared 36-byte vertices, 16-bit indices,
then fixed 92/96-byte material records. Unknown fields are not assigned semantics.
"""
import json
import math
from pathlib import Path
import struct
import sys
from mod_bundle.retail import Retail


def classify(data):
    if not data:
        return {"kind": "empty"}
    if len(data) < 52:
        return {"kind": "unclassified", "reason": "short header"}
    bounds = struct.unpack_from('<11f', data)
    count = struct.unpack_from('<I', data, 44)[0]
    if not all(map(math.isfinite, bounds)) or bounds[3] != 1 or count > (len(data)-52)//36:
        return {"kind": "unclassified", "reason": "bounds/vertices"}
    offset = 48 + 36 * count
    indices = struct.unpack_from('<I', data, offset)[0]
    offset += 4
    if indices > (len(data)-offset-4)//2:
        return {"kind": "unclassified", "reason": "index bounds"}
    values = struct.unpack_from(f'<{indices}H', data, offset)
    if any(index != 65535 and index >= count for index in values):
        return {"kind": "unclassified", "reason": "index outside vertex pool"}
    offset += indices * 2
    parts = struct.unpack_from('<I', data, offset)[0]
    offset += 4
    candidates = []
    for stride in (92, 96):
        if not parts or len(data)-offset != parts*stride:
            continue
        names = []
        for part in range(parts):
            at = offset + part*stride
            name = data[at:at+32].split(b'\0',1)[0]
            start, length, vertex_start, vertex_count = struct.unpack_from('<4I',data,at+32)
            if not name or any(c < 32 or c > 126 for c in name):
                break
            if start+length > indices or vertex_start+vertex_count > count:
                break
            names.append(name.decode('ascii'))
        else:
            candidates.append({"record_bytes":stride,"materials":names})
    return {"kind":"legacy_candidate" if len(candidates)==1 else "unclassified",
            "vertices":count,"indices":indices,"parts":parts,"candidates":candidates}


def main():
    retail = Retail(Path(sys.argv[1]))
    rows = []
    for entry in json.loads(Path(sys.argv[2]).read_text()):
        raw = retail.read(entry['path'])
        rows.append({"path":entry['path'],"size":len(raw),**classify(raw)})
    counts = {kind:sum(row['kind']==kind for row in rows) for kind in sorted({r['kind'] for r in rows})}
    Path(sys.argv[3]).write_text(json.dumps({"counts":counts,"files":rows},indent=2)+'\n')
    print(counts)


if __name__ == '__main__':
    main()
