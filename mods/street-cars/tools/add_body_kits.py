"""Append study kits 2/3 losslessly to a fresh runtime package candidate.

python THIS REPO BASELINE_RUNTIME STUDY_DIR EMPTY_CANDIDATE
No original/native part, texture, handling recipe, wheel or old index is replaced.
"""
import hashlib
import json
import re
import shutil
import struct
import sys
from pathlib import Path

import numpy as np

repo, baseline, study, output = map(Path, sys.argv[1:])
sys.path[:0] = [str(repo/'tools'), str(repo/'mods/street-cars/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import mesh_bytes, bounds

if output.exists():
    raise RuntimeError('Candidate must not exist')
shutil.copytree(baseline, output)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def combine(original, additions):
    """Retain every old material/vertex byte and old local index verbatim."""
    base = mesh.read(original)
    extra = mesh.read(mesh_bytes(additions))
    indices = np.concatenate([base['indices'], extra['indices']]).astype('<u2')
    data = struct.pack('<I', len(indices)) + indices.tobytes()
    data += struct.pack('<I', len(base['parts']) + len(extra['parts']))
    for part in base['parts']:
        data += bytes(part['header']) + bytes(part['verts'])
    for part in extra['parts']:
        header = bytearray(part['header'])
        struct.pack_into('<I', header, 32, part['first'] + len(base['indices']))
        data += header + part['verts']
    data += bounds(np.concatenate([p['streams'][0, 0] for p in base['parts']+extra['parts']]))
    reread = mesh.read(data)
    assert np.array_equal(reread['indices'][:len(base['indices'])], base['indices'])
    for old, new in zip(base['parts'], reread['parts']):
        assert old['header'] == new['header'] and old['verts'] == new['verts']
    return data


catalogue = output/'patches/scripts/cardefs.txt.patch'
text = bytes.fromhex(catalogue.read_text().split('text ')[1].strip()).decode()
receipt = dict(baseline=str(baseline), study=str(study), output=str(output),
               unchanged_existing_indices=[0, 1], added_indices=[2, 3], meshes=[], exports={})
for model, prefix in [('integra_db8', 'db8'), ('bmw_e36', 'e36'), ('levin_ae92', 'ae92')]:
    block_match = re.search(r'\[BeginCar\]\s+'+model+r'\b.*?\[EndCar\]', text, re.I | re.S)
    if not block_match:
        raise RuntimeError('Missing car '+model)
    block = block_match[0]
    old_block = block
    for level in [2, 3]:
        source = study/(model+'-kit'+str(level)+'.glb')
        receipt['exports'][source.name] = sha(source)
        gltf = Gltf(source)
        study_report = json.loads((study/(model+'-kit-study.json')).read_text())
        notch = next(p['exhaust_clearance_x_m'] for p in study_report['parts']
                     if p['part'] == 'REAR' and p['level'] == level)
        zones = dict(FBUMP=[], RBUMP=[], SSKIRT_L=[], SSKIRT_R=[])
        for p in gltf.parts():
            path = p['source'].upper()
            key = ('FBUMP' if 'FRONT_' in path else 'RBUMP' if 'REAR_' in path else
                   'SSKIRT_L' if 'SKIRT_L_' in path else 'SSKIRT_R' if 'SKIRT_R_' in path else None)
            if key is None:
                raise RuntimeError('Unclassified export '+path)
            p['pos'] *= 100
            mat = gltf.j['materials'][p['mi']]['name']
            assert mat in [model+'_body', prefix+'_kit_trim'], mat
            p['mat'] = mat
            if key == 'RBUMP':
                assert p['pos'][:, 0].max() <= notch[0]*100+.01 or p['pos'][:, 0].min() >= notch[1]*100-.01, (
                    'new valance blocks the stock exhaust opening', path)
            assert np.isfinite(p['pos']).all() and np.isfinite(p['norm']).all()
            assert np.allclose(np.linalg.norm(p['norm'], axis=1), 1, atol=1e-4)
            a, b, c = p['pos'][p['tri']].transpose(1, 0, 2)
            assert np.all(np.linalg.norm(np.cross(b-a, c-a), axis=1) > 1e-8)
            # Validate the actual exported bevel, including sealed end caps.
            unique, inverse = np.unique(np.round(p['pos'], 4), axis=0, return_inverse=True)
            tri = inverse[p['tri']]
            edges = np.sort(np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]]), axis=1)
            _, counts = np.unique(edges, axis=0, return_counts=True)
            assert np.all(counts == 2), ('open/nonmanifold exported kit', path)
            assert p['pos'][:, 1].min() > 9, ('ground clearance', path)
            zones[key].append(p)
        for zone, parts in zones.items():
            assert parts, (model, zone)
            for suffix in ['', '_ui']:
                folder = output/'assets/cars'/(model+suffix)/'mesh'
                stock = folder/(prefix+'_'+zone.lower()+'.d8m')
                target = folder/(prefix+'_kit'+str(level)+'_'+zone.lower()+'.d8m')
                assert not target.exists()
                target.write_bytes(combine(stock.read_bytes(), parts))
                receipt['meshes'].append(dict(path=str(target.relative_to(output)), sha256=sha(target),
                                             preserved_stock=sha(stock),
                                             added_triangles=sum(len(p['tri']) for p in parts)))
        for category, zone in [('FBUMP', 'FBUMP'), ('RBUMP', 'RBUMP'), ('SKIRT', 'SSKIRT')]:
            block = block.replace('[EndCar]', f'CUSTOM {category} "{prefix}_kit{level}_{zone}" AERO\n[EndCar]')
    text = text.replace(old_block, block)
    for suffix in ['', '_ui']:
        (output/'assets/cars'/(model+suffix)/'mesh/mats'/(prefix+'_kit_trim.pmd')).write_text(
            'Material Program = Constant Colour\nColour = 25 25 27\n')
catalogue.write_text('append\ntext '+text.encode().hex()+'\n')
ini = output/'mod.ini'
assert 'revision=4\n' in ini.read_text()
ini.write_text(ini.read_text().replace('revision=4\n', 'revision=5\n'))
changed = []
for source in baseline.rglob('*'):
    if not source.is_file():
        continue
    rel = source.relative_to(baseline)
    if sha(source) != sha(output/rel):
        changed.append(rel.as_posix())
assert sorted(changed) == ['mod.ini', 'patches/scripts/cardefs.txt.patch'], changed
receipt['existing_files_changed'] = changed
manifest = {p.relative_to(output).as_posix(): sha(p) for p in sorted(output.rglob('*'))
            if p.is_file() and p.name != 'SHA256.json'}
(output/'SHA256.json').write_text(json.dumps(manifest, indent=2)+'\n')
(study/'candidate-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(dict(meshes=len(receipt['meshes']), existing_files_changed=changed,
                      dynamics_hoods_decals_wheels_stock_sport_unchanged=True)))
