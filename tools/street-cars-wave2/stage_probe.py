"""Assemble an isolated overlay; never write through input or output symlinks."""
from pathlib import Path
import argparse

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('base',type=Path);p.add_argument('probe',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
 base,probe,out=[x.resolve() for x in (a.base,a.probe,a.output)]
 for source in [base,probe]:
  if out==source or out.is_relative_to(source) or source.is_relative_to(out):p.error('Output must be disjoint from inputs')
  for name in ['cardefs.txt','mod_cars.txt']:
   if not (source/'scripts'/name).is_file():p.error('Missing input catalogue: '+str(source/name))
 # File-level links are safe to unlink; directory links would redirect writes into the source.
 if a.output.is_symlink() or (out.exists() and any(x.is_symlink() and x.is_dir() for x in out.rglob('*'))):
  p.error('Output contains directory symlinks; use a fresh isolated directory')
 additive={'cardefs.txt','mod_cars.txt','mod_unlocks.txt','arcadeunlock.txt','arcadecollection.txt'}
 expected=set()
 for source in [base,probe]:
  for f in source.rglob('*'):
   if not f.is_file():continue
   relative=f.relative_to(source);expected.add(relative);dest=out/relative;dest.parent.mkdir(parents=True,exist_ok=True)
   if dest.is_symlink() or dest.exists():dest.unlink()
   if source==probe and relative.parent==Path('scripts') and f.name in additive:
    original=base/relative;dest.write_text((original.read_text() if original.exists() else '')+'\n'+f.read_text())
   else:dest.symlink_to(f)
 # A removed generated file must not survive a successful rebuild as stale content.
 for dest in out.rglob('*'):
  if (dest.is_file() or dest.is_symlink()) and dest.relative_to(out) not in expected:dest.unlink()
 print(out)
if __name__=='__main__':main()
