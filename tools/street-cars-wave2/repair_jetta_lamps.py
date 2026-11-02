"""Restore the retained Jetta source's displaced stock headlamp assemblies.

Blender CLI: -- SOURCE_BLEND PREPARED_BLEND FRESH_OUTPUT_DIRECTORY
Source files are read only. Native renders must approve the resulting candidate.
"""
import json
import hashlib
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

source, prepared, output = map(Path, sys.argv[sys.argv.index('--') + 1:])
output.mkdir(parents=True, exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(source))
assemblies = {
    'jettaa5_headlightframe_R', 'jettaa5_headlightframe_L',
    'jettaa5_headlight_R', 'jettaa5_headlight_L',
    'jettaa5_headlight_R2', 'jettaa5_headlight_L2',
    'jettaa5_headlightglass_R', 'jettaa5_headlightglass_L',
}
pieces = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH' or not obj.parent or obj.parent.name not in assemblies:
        continue
    uv = obj.data.uv_layers.active
    points = [tuple(obj.matrix_world @ v.co) for v in obj.data.vertices]
    pieces.append(dict(name=obj.name, assembly=obj.parent.name, vertices=points,
                       faces=[tuple(p.vertices) for p in obj.data.polygons],
                       uv=[tuple(uv.data[i].uv) for i in range(len(obj.data.loops))] if uv else None,
                       material=obj.data.materials[0].name))
assert len(pieces) == 14, f'Unexpected pinned source assemblies: {len(pieces)}'

bpy.ops.wm.open_mainfile(filepath=str(prepared))

def material(name, rgb, metallic=0, alpha=1):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    node = mat.node_tree.nodes['Principled BSDF']
    node.inputs['Base Color'].default_value = (*rgb, 1)
    node.inputs['Metallic'].default_value = metallic
    node.inputs['Roughness'].default_value = .22
    node.inputs['Alpha'].default_value = alpha
    mat.diffuse_color = (*rgb, alpha)
    if alpha < 1:
        mat.surface_render_method = 'DITHERED'
    return mat

chrome = material('jetta_stock_reflector', (.62, .65, .68), .8)
amber = material('jetta_stock_indicator', (.85, .31, .035))
bulb = bpy.data.materials.get('oj_head') or material('oj_head', (.8, .83, .85), .3)
lens = material('jetta_clear_headlamp_lens', (.7, .8, .85), alpha=.12)
replaced = []
for obj in list(bpy.context.scene.objects):
    if obj.type == 'MESH' and obj.get('source_assembly', '').lower() in {
        name.lower() for name in assemblies
    }:
        replaced.append(obj.name)
        bpy.data.objects.remove(obj, do_unlink=True)
assert len(replaced) == 2  # the two outer lenses

# The simplified trunk liner penetrates the rear body/window and makes two
# floating black fins. Keep that interior mesh inside the actual exterior shell.
vertices, faces = [], []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH' or not any(
        m and m.name in {'oj_paint', 'oj_glass'} for m in obj.data.materials
    ):
        continue
    start = len(vertices)
    vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
    faces.extend([start + i for i in p.vertices] for p in obj.data.polygons)
shell = BVHTree.FromPolygons(vertices, faces)
liner = bpy.data.objects.get('jettaa5_carpet-material')
assert liner is not None and liner.get('source_assembly')
inverse = liner.matrix_world.inverted()
fitted = []
for vertex in liner.data.vertices:
    point = liner.matrix_world @ vertex.co
    hit, _, _, _ = shell.ray_cast(Vector((point.x, point.y, 3)), Vector((0, 0, -1)), 5)
    if hit is not None and point.z > hit.z - .008:
        before = tuple(point)
        point.z = hit.z - .008
        vertex.co = inverse @ point
        fitted.append(dict(vertex=vertex.index, before=before, after=tuple(point)))
assert fitted, 'Expected the retained source liner to intersect the exterior'
liner.data.update()

for part in pieces:
    # The donor scene stacks optional assemblies vertically. Its two stock
    # frames sit exactly 1.68 m above their fitted outer lenses (z .60 .. .79).
    displaced = part['assembly'] not in {
        'jettaa5_headlightglass_R', 'jettaa5_headlightglass_L'
    }
    points = [(x, y, z - (1.68 if displaced else 0)) for x, y, z in part['vertices']]
    if displaced or 'headlightglass' in part['assembly']:
        assert .58 < min(p[2] for p in points) < .80
        assert max(p[2] for p in points) < .81
    mesh = bpy.data.meshes.new('stock_' + part['name'])
    mesh.from_pydata(points, [], part['faces'])
    mesh.update()
    if part['uv']:
        layer = mesh.uv_layers.new()
        assert len(layer.data) == len(part['uv'])
        for dst, src in zip(layer.data, part['uv']):
            dst.uv = src
    obj = bpy.data.objects.new('restored_' + part['name'], mesh)
    bpy.context.collection.objects.link(obj)
    mat = lens if 'headlightglass' in part['assembly'] else (
        bulb if 'highbeam' in part['name'] else (
        amber if any(x in part['name'] for x in ['signal', 'parkinglight']) else chrome)
    )
    mesh.materials.append(mat)
    for poly in mesh.polygons:
        poly.use_smooth = True

bpy.ops.wm.save_as_mainfile(filepath=str(output / 'jetta-vento-mk5-prepared.blend'))
bpy.ops.export_scene.gltf(filepath=str(output / 'jetta-vento-mk5-prepared.glb'),
                          export_format='GLB', export_yup=True)
(output / 'lamp-restoration.json').write_text(json.dumps(dict(
    source=str(source), prepared=str(prepared), replaced_objects=replaced,
    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    prepared_sha256=hashlib.sha256(prepared.read_bytes()).hexdigest(),
    trunk_liner_fitted_vertices=fitted,
    restored=[dict(name=p['name'], assembly=p['assembly'], polygons=len(p['faces']),
                   displacement_m=[0, 0, 0 if p['assembly'] in {
                       'jettaa5_headlightglass_R', 'jettaa5_headlightglass_L'} else -1.68]) for p in pieces],
    status='source-derived candidate; native visual review required'), indent=2) + '\n')
