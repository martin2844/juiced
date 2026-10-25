"""Build independent native sound banks from Edvvc's licensed Ferrari 458 recording.

The derived audio is CC BY-SA 3.0. This is a short exterior recording, not a
studio dynamometer library: on/off and sport variants are explicitly processed.
"""
import hashlib
from pathlib import Path
import struct
import subprocess
import wave
import numpy as np

RATE = 48000
SOURCE_SHA256 = 'd98951caaa3de712e59b13d4d1eb0472795a3cbfbf9d990b28711cd8c93891da'


def loop(source, start, duration, sport=False, off=False):
    sample=source[int(start*RATE):int((start+duration)*RATE)].copy()
    sample-=sample.mean()
    # Remove wind/rumble and soften the distant crowd without adding synthetic tones.
    f=np.fft.rfftfreq(len(sample),1/RATE)
    highpass=1-np.exp(-(f/65)**4)
    lowpass=1/(1+(f/(2400 if off else 6500))**4)
    sample=np.fft.irfft(np.fft.rfft(sample)*highpass*lowpass,n=len(sample))
    # Equal-power seam with endpoint matching; no clicks at native looping boundaries.
    cross=int(.045*RATE)
    weight=.5-.5*np.cos(np.linspace(0,np.pi,cross))
    seam=sample[-cross:]*(1-weight)+sample[:cross]*weight
    sample=np.r_[seam,sample[cross:-cross]]
    sample-=np.linspace(0,sample[-1]-sample[0],len(sample))
    sample*=.16/max(np.sqrt(np.mean(sample*sample)),1e-9)
    if sport:sample=np.tanh(sample*1.8)/1.8
    sample*=min(1,.88/max(abs(sample)))
    if off:sample*=.7
    return np.round(sample*32767).astype('<i2')


def build_audio(root):
    assets=Path(__file__).parent/'assets'
    source=assets/'ferrari-458-edvvc.ogg'
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    if digest!=SOURCE_SHA256:raise ValueError('Ferrari audio source checksum mismatch')
    raw=subprocess.run(['ffmpeg','-v','error','-i',str(source),'-f','f32le','-ac','1','-ar',str(RATE),'-'],
                       check=True,capture_output=True).stdout
    samples=np.frombuffer(raw,'<f4').astype(float)
    # Source harmonic estimates give approximately 1500 / 2050 / 3900 / 4500 RPM.
    # The game's native RPM resampling extends this recording to the 9,000 RPM redline.
    layers=[('idle',.55,.65,1500,2300,0,2250),('low',1.94,.35,2050,4000,1200,3800),
            ('mid',3.38,.24,3900,6500,2800,6000),('high',4.03,.30,4500,10000,4800,10000)]
    report={}
    for bank,sport in [('f458',False),('f458b',True)]:
        directory=root/'audio/cars'/bank;directory.mkdir(parents=True,exist_ok=True)
        entries=[]
        for label,start,duration,recorded_rpm,rpm,lo,hi in layers:
            for mode in ([''] if label=='idle' else ['_on','_off']):
                sample_name=label+mode
                pcm=loop(samples,start,duration,sport,mode=='_off')
                # Native 3D voices cap playback at 48 kHz. Prepare each layer at its
                # upper RPM so runtime only slows it down, avoiding a flat pitch at redline.
                length=round(len(pcm)*recorded_rpm/rpm)
                pcm=np.round(np.interp(np.linspace(0,len(pcm)-1,length),np.arange(len(pcm)),pcm)).astype('<i2')
                with wave.open(str(directory/(sample_name+'.wav')),'wb') as out:
                    out.setnchannels(1);out.setsampwidth(2);out.setframerate(RATE);out.writeframes(pcm.tobytes())
                entries.append(sample_name)
                report[bank+'/'+sample_name]={'frames':len(pcm),'peak':int(max(abs(pcm.astype(int)))),
                                             'seam_delta':int(abs(int(pcm[0])-int(pcm[-1])))}
        cfg=['[BeginBank]','bank_size 7','instance_count 7','bank_name '+bank,'banktype 3','[BeginSamples]']
        cfg+=['declare_sample '+s for s in entries]+['[EndSamples]']
        for s in entries:
            cfg+=['[BeginSS]','name '+bank+'_'+s,'type 3','sample '+s,'instances 1','[EndSS]']
        cfg+=['[EndBank]']
        (directory/(bank+'.cfg')).write_text('\n'.join(cfg)+'\n')
        rev=[];vol=[]
        for mode in range(3):
            for i,(_,_,_,_,rpm,lo,hi) in enumerate(layers):
                rev.extend((lo,hi,rpm))
                vol.extend(([0,.8,.8,0] if mode<2 else ([.75,.75,.85,0] if i==0 else [0,1,1,1] if i==1 else [0,0,0,0]))+[lo,lo,hi,hi])
        (directory/(bank+'.rev')).write_bytes(struct.pack('<36f',*rev))
        (directory/(bank+'.vol')).write_bytes(struct.pack('<96f',*vol))
    (root/'AUDIO-ATTRIBUTION.txt').write_bytes((assets/'AUDIO-ATTRIBUTION.txt').read_bytes())
    (root/'CC-BY-SA-3.0.txt').write_bytes((assets/'CC-BY-SA-3.0.txt').read_bytes())
    return {'source_sha256':digest,'banks':report,'license':'CC-BY-SA-3.0'}
