"""Blender BVH occlusion check of exported opaque cavity, not authoring objects."""
from pathlib import Path
import sys,json,math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
r,base,candidate,out=map(Path,sys.argv[sys.argv.index('--')+1:]);sys.path.insert(0,str(r/'tools'));from vinyl_pack import mesh
rel=Path('assets/cars/peugeot_405_glx/mesh/w257_chass.d8m');old=mesh.read((base/rel).read_bytes());new=mesh.read((candidate/rel).read_bytes())
def tree_for(root):
 v=[];f=[]
 for zone in ['chass','bonnet','fbump']:
  aa=mesh.read((root/'assets/cars/peugeot_405_glx/mesh'/('w257_'+zone+'.d8m')).read_bytes())
  for p in aa['parts']:
   offset=len(v);v.extend(p['streams'][0,0]);f.extend((mesh.triangles(aa,p)+offset).tolist())
 return BVHTree.FromPolygons(v,f,all_triangles=True)
tree=tree_for(candidate);negative=tree_for(base);hits=[]
def front_hit(tree,origin,direction):
 remaining=70.0
 for _ in range(30):
  hit,normal,_,distance=tree.ray_cast(origin,direction,remaining)
  if hit is None:return None
  if normal.dot(direction)<-1e-6:return hit
  # The GPU culls a back-facing plane: continue past it, rather than count
  # a two-sided BVH hit as an opaque surface. Original bonnet/body faces also
  # close samples above or outside an actual grille aperture.
  remaining-=distance+.002
  if remaining<=0:return None
  origin=hit+direction*.002
 return None
for x in [-38,-25,-12,0,12,25,38]:
 for y in [28,40,55,60,65,68]:
  for angle in [-45,0,45]:
   theta=math.radians(angle);direction=Vector((math.sin(theta),0,-math.cos(theta)))
   origin=Vector((x,y,205));hit=front_hit(tree,origin,direction)
   assert hit is not None,('open cavity',x,y,angle)
   before=front_hit(negative,origin,direction)
   hits.append(dict(x=x,y=y,angle=angle,hit=list(hit),r19_open=before is None))
open_before=sum(h['r19_open'] for h in hits);assert open_before>0,'Negative control must demonstrate the original open cavity'
out.write_text(json.dumps(dict(rays=len(hits),all_intercepted_by_exported_front_facing_opaque_geometry=True,baseline_open_rays=open_before,coordinates='Native cm',samples=hits),indent=2)+'\n');print('All',len(hits),'rays blocked with GPU-style backface rejection;',open_before,'were open in r19.')
