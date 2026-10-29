"""Engineering candidate, not a certified vehicle simulation.
Factory power/torque anchors are distinct from provisional gearing/chassis tuning.
Run the native handling diagnostic after every change.
"""
from pathlib import Path
import json,re,math,sys
root=Path(sys.argv[1]);spec=json.loads(Path(__file__).with_name('specifications.json').read_text())['cars']
# Ratios marked provisional must be checked against the selected market/gearbox.
settings=[
 (76/.745699872,153,6000,3000,1100,[3.455,1.85,1.28,.969,.757],4.18),
 (194*.73549875/.745699872,267,5500,4200,1458,[1/.325,1/.5609,1/.8378,1/1.1081,1/1.3636],69/16),
 (184/.745699872,320,6300,2800,1510,[3.357,2.087,1.469,1.15,1.194*3.087/3.944,.975*3.087/3.944],3.944),
 (110/.745699872,200,6000,3500,1398,[3.778,2.118,1.36,1.029,.837,.733],3.684),
 (165,166*1.355817948,5600,4000,3425*.45359237,[3.454,2.062,1.448,1.088,.871],4.111),
 (114/.745699872,240,5500,3750,1330,[3.71,2.16,1.37,1,.797],3.73),
 (168,166*1.355817948,6000,4500,3488*.45359237,[3.385,1.905,1.194,.868,.7],4.25),
]
reports=[]
for s,(bhp,torque,peakrpm,torquerpm,mass,gears,final) in zip(spec,settings):
 path=root/'scripts'/('dynamic_'+s['model'].lower()+'.txt');text=path.read_text()
 # Piecewise approximate stock curve constrained by the factory peak torque and power.
 # Decline above rated power; do not retain a donor's high-rpm power.
 idle=750+peakrpm%250
 speeds=sorted(set([idle,1500,torquerpm,peakrpm,peakrpm+500,peakrpm+900]))
 curve=[]
 for rpm in speeds:
  if rpm<=torquerpm:f=.45+.55*(rpm-idle)/(torquerpm-idle)
  elif rpm<=peakrpm:
   at_power=bhp*745.699872/(peakrpm*math.pi/30)/torque
   f=1+(at_power-1)*(rpm-torquerpm)/(peakrpm-torquerpm)
  else:f=(bhp*745.699872/(rpm*math.pi/30)/torque)*max(0,1-(rpm-peakrpm)/900)
  curve.append(min(f,bhp*745.699872/(rpm*math.pi/30)/torque))
 changes={'Mass':mass,'MaxTorque':torque,'TorqueCurveX':' '.join(map(str,speeds)),'TorqueCurveY':' '.join(f'{x:.7f}' for x in curve),'ForwardGearRatios':' '.join(f'{x:.6f}' for x in gears),'FinalDrive':final,'PowerRangeMin':torquerpm,'PowerRangeMax':peakrpm}
 for k,v in changes.items():text=re.sub(r'^'+k+r'[^\n]*',k+' '+str(v),text,flags=re.M)
 # The roster drivetrain label does not configure the actual driven wheels.
 def wheel(m):
  block=m[0];idx=int(re.search(r'WheelId\s+(\d+)',block)[1]);driven=s['drive']=='AWD' or (idx<2)==(s['drive']=='FWD')
  block=re.sub(r'^Drive[^\n]*\n','',block,flags=re.M)
  return block.replace('[EndWheel]',f'Drive {int(driven)}\n[EndWheel]')
 text=re.sub(r'\[BeginWheel\].*?\[EndWheel\]',wheel,text,flags=re.S)
 path.write_text(text)
 reports.append(dict(model=s['model'],bhp_target=bhp,torque_nm=torque,mass_kg=mass,drive=s['drive'],gears=gears,final_drive=final,status='candidate: verify gearing, mass conventions, acceleration, braking and cornering before release'))
(root/'reports/handling-candidates.json').write_text(json.dumps(reports,indent=2))
