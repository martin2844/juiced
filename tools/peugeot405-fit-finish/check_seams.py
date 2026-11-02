"""Blender read-back measurement of the actual exported kit-to-body contact.
Blender --background --python THIS -- REPO PACKAGE RECEIPT
"""
from pathlib import Path
import sys,json
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
r,package,dest=map(Path,sys.argv[sys.argv.index('--')+1:]);sys.path.insert(0,str(r/'tools'))
from vinyl_pack import mesh
folder=package/'assets/cars/peugeot_405_glx/mesh'
fixed=mesh.read((folder/'w257_chass.d8m').read_bytes());verts=[];faces=[]
for p in fixed['parts']:
 if p['material'].startswith('p405_'):continue
 off=len(verts);verts.extend(p['streams'][0,0]);faces.extend((mesh.triangles(fixed,p)+off).tolist())
tree=BVHTree.FromPolygons(verts,faces,all_triangles=True);rows=[]
for zone,seam in [('fbump',58.25),('rbump',64.5)]:
 for kit in [1,2,3]:
  m=mesh.read((folder/f'w257_kit{kit}_{zone}.d8m').read_bytes());top=[]
  for p in m['parts']:
   if p['material']!='peugeot_405_glx_body':continue
   v=p['streams'][0,0];top.extend(v[abs(v[:,1]-seam)<.04])
  assert len(top)>100,(zone,kit,len(top))
  v=np.unique(np.round(top,5),axis=0);dist=np.array([tree.find_nearest(Vector(p))[3] for p in v])
  rows.append(dict(zone=zone,kit=kit,points=len(v),maximum_gap_cm=float(dist.max()),median_gap_cm=float(np.median(dist)),worst_point=v[dist.argmax()].tolist()))
result=dict(measurement='Nearest fixed-body surface to actual exported mounting vertices; not a race or damage-state test',rows=rows)
dest.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
assert max(x['maximum_gap_cm'] for x in rows)<.12,'A visible mounting gap remains'
