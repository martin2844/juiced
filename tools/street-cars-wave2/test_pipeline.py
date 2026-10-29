"""Failure-path regressions for isolated staging and screenshot evidence."""
import subprocess,sys,tempfile,unittest
from pathlib import Path
SCRIPTS=Path(__file__).resolve().parent
class PipelineTests(unittest.TestCase):
 def run_tool(self,name,*args):
  return subprocess.run([sys.executable,str(SCRIPTS/name),*map(str,args)],capture_output=True,text=True)
 def catalogue(self,root):
  (root/'scripts').mkdir(parents=True)
  for name in ['cardefs','mod_cars','arcadeunlock','arcadecollection']:
   (root/'scripts'/(name+'.txt')).write_text('Car FERRARI_458\n')
 def test_missing_base_does_not_create_stage(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);self.catalogue(r/'probe')
   result=self.run_tool('stage_probe.py',r/'missing',r/'probe',r/'stage')
   self.assertNotEqual(result.returncode,0);self.assertFalse((r/'stage').exists())
 def test_directory_link_cannot_redirect_stage_writes(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);self.catalogue(r/'base');self.catalogue(r/'probe');(r/'stage').mkdir()
   (r/'stage/scripts').symlink_to(r/'base/scripts',target_is_directory=True)
   original=(r/'base/scripts/cardefs.txt').read_bytes()
   result=self.run_tool('stage_probe.py',r/'base',r/'probe',r/'stage')
   self.assertNotEqual(result.returncode,0);self.assertEqual((r/'base/scripts/cardefs.txt').read_bytes(),original)
 def test_rebuild_drops_removed_asset_without_editing_base(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);self.catalogue(r/'base');self.catalogue(r/'probe')
   asset=r/'probe/obsolete';asset.write_text('old')
   self.assertEqual(self.run_tool('stage_probe.py',r/'base',r/'probe',r/'stage').returncode,0)
   asset.unlink()
   self.assertEqual(self.run_tool('stage_probe.py',r/'base',r/'probe',r/'stage').returncode,0)
   self.assertFalse((r/'stage/obsolete').is_symlink())
   self.assertEqual((r/'base/scripts/cardefs.txt').read_text(),'Car FERRARI_458\n')
 def test_failed_capture_rejects_old_evidence(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp);self.catalogue(r/'qa-stage')
   for ext in ['bmp','png']:(r/('TEST-stock.'+ext)).write_bytes(b'old screenshot')
   result=self.run_tool('capture_native.py','--work',r,'--game',r,'--cars','TEST','--executable','/bin/false')
   self.assertNotEqual(result.returncode,0)
   self.assertFalse((r/'TEST-stock.bmp').exists());self.assertFalse((r/'TEST-stock.png').exists())
 def test_archive_cannot_include_itself(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp)
   result=self.run_tool('package.py','--source',r/'source','--output',r/'package','--game',r,'--model-sources',r,'--archive',r/'package/mod.zip')
   self.assertNotEqual(result.returncode,0);self.assertIn('Archive must be outside',result.stderr)
   self.assertFalse((r/'package').exists())
 def test_incomplete_package_is_not_published(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=Path(tmp)
   result=self.run_tool('package.py','--source',r/'missing','--output',r/'package','--game',r,'--model-sources',r)
   self.assertNotEqual(result.returncode,0);self.assertFalse((r/'package').exists())
if __name__=='__main__':unittest.main()
