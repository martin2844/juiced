"""Classify the retained source tailpipe as the native replaceable exhaust.

Blender CLI: -- PREPARED_BLEND FRESH_OUTPUT. Geometry/UV/materials stay exact.
"""
from pathlib import Path
import bpy,sys,json,hashlib,struct
source,out=map(Path,sys.argv[sys.argv.index('--')+1:])
assert source.is_file() and not out.exists() and out.parent.resolve()!=source.parent.resolve()
original_hash=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source))
def geometry(o):
    h=hashlib.sha256()
    for v in o.data.vertices:
        h.update(struct.pack('<3f',*(o.matrix_world@v.co)))
    for p in o.data.polygons:
        h.update(struct.pack('<II',p.material_index,len(p.vertices)))
        h.update(struct.pack('<'+'I'*len(p.vertices),*p.vertices))
    for uv in o.data.uv_layers:
        h.update(uv.name.encode())
        for value in uv.data:h.update(struct.pack('<2f',*value.uv))
    for n in o.data.corner_normals:h.update(struct.pack('<3f',*n.vector))
    h.update(json.dumps([m.name if m else None for m in o.data.materials]).encode())
    return h.hexdigest()
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
before={o.name:geometry(o) for o in objects}
tail=bpy.data.objects.get('pCube24_Subaru_Outback_0')
assert tail and tail.type=='MESH' and len(tail.data.polygons)==284
assert bpy.data.objects.get('exhaust') is None
material_counts={m.name:sum(p.material_index==i for p in tail.data.polygons) for i,m in enumerate(tail.data.materials) if m}
# The source pipe must not contribute paint/vinyl receiver faces after it is detached.
assert material_counts.get('oj_paint',0)==0,material_counts
world=tail.matrix_world.copy()
group=bpy.data.objects.new('exhaust',None)
bpy.context.collection.objects.link(group)
tail.parent=group;tail.matrix_world=world
bpy.context.view_layer.update()
after={o.name:geometry(o) for o in objects}
assert before==after
out.mkdir()
blend=out/'subaru-outback-2002-prepared.blend';glb=out/'subaru-outback-2002-prepared.glb'
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
bpy.ops.export_scene.gltf(filepath=str(glb),export_format='GLB',export_yup=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==original_hash
(out/'exhaust-classification.json').write_text(json.dumps(dict(source=str(source),source_sha256=original_hash,
    output_blend=str(blend),output_glb=str(glb),tailpipe=tail.name,polygons=284,material_counts=material_counts,
    all_objects_geometry_uv_materials_normals_identical=before==after,object_count=len(objects),
    before=before,after=after,scope='Hierarchy-only classification of original source exhaust. No invented geometry, relocation, texture, physics, or source overwrite. Native candidate validation still required.'),indent=2)+'\n')
print('Classified original284-polygon tailpipe; all74 mesh attributes unchanged')
