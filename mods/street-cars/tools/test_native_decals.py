"""Geometric regression checks, independent of rendering and user saves."""
import unittest
import sys
from pathlib import Path
import numpy as np
from native_decals import intersect,cross
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools/mod_bundle'))
from import_cars import split_bonnet

class FootprintTests(unittest.TestCase):
 def test_intersection_preserves_surface_and_uv_affine_coordinates(self):
  # Donor fills a large triangle. Receiver is one small panel in its interior.
  donor=np.array([[0,0,0,0],[4,0,1,0],[0,4,0,1]],dtype=float)
  receiver=np.array([[1,1],[2,1],[1,2]],dtype=float)
  polygon=np.array(intersect(donor,receiver))
  self.assertEqual(len(polygon),3)
  np.testing.assert_allclose(polygon[:,2:],polygon[:,:2]/4)
  area=abs(cross(polygon[1,:2]-polygon[0,:2],polygon[2,:2]-polygon[0,:2]))/2
  self.assertAlmostEqual(float(area),.5)
 def test_window_gap_has_no_geometry(self):
  donor=np.array([[0,0,0,0],[1,0,1,0],[0,1,0,1]],dtype=float)
  self.assertEqual(intersect(donor,np.array([[2,0],[3,0],[2,1]],float)),[])
 def test_winding_does_not_change_coverage(self):
  donor=np.array([[0,0,0,0],[4,0,1,0],[0,4,0,1]],float)
  receiver=np.array([[1,1],[2,1],[1,2]],float)
  a=np.array(intersect(donor,receiver));b=np.array(intersect(donor,receiver[::-1]))
  self.assertEqual({tuple(x.round(6)) for x in a},{tuple(x.round(6)) for x in b})
 def test_hood_partition_keeps_surrounding_body_without_overlap(self):
  p=dict(pos=np.array([[-1,1,-1],[1,1,-1],[1,1,1],[-1,1,1]],float),norm=np.tile([0,1,0],(4,1)),uv=np.zeros((4,2)),tri=np.array([[0,2,1],[0,3,2]]),mat='body')
  parts=split_bonnet(p,[[-.5,-.5],[.5,-.5],[.5,.5],[-.5,.5]],1)
  areas={}
  for part in parts:
   v=part['pos'][part['tri']]
   areas[part['bonnet']]=float(np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1).sum()/2)
  self.assertAlmostEqual(areas[True],1)
  self.assertAlmostEqual(areas[False],3)
if __name__=='__main__':unittest.main()
