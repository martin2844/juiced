"""Keep original exterior panel boundaries: unconstrained collapse opens panel seams."""
import bpy,sys
from pathlib import Path
r=Path(sys.argv[sys.argv.index('--')+1]);p=r/'jetta-vento-mk5-repaired.blend'
bpy.ops.wm.open_mainfile(filepath=str(p))
targets={o.name:o for o in bpy.context.scene.objects if o.type=='MESH' and any(m and m.name.startswith('oj_paint') for m in o.data.materials)}
with bpy.data.libraries.load(str(r/'jetta-vento-mk5-pre-optimization.blend'),link=False) as (src,dst):dst.objects=list(targets)
for name,ref in zip(targets,dst.objects):
 if ref is None:continue
 if name:
  target=targets[name];target.data=ref.data.copy();target.data.materials.clear();target.data.materials.append(bpy.data.materials['oj_paint']);print('RESTORED PANEL',name,len(target.data.polygons))
 bpy.data.objects.remove(ref,do_unlink=True)
bpy.context.scene['exterior_seams_preserved']=True
bpy.ops.wm.save_as_mainfile(filepath=str(p))
