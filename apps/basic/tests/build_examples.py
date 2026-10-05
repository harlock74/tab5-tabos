#!/usr/bin/env python3
"""Generate tokenized examples with the production interpreter, and smoke-run them."""
from pathlib import Path
import shutil
import sys
import tempfile
import time
from session import Session

executable=str(Path(sys.argv[1]).resolve())
output=Path(sys.argv[2]) if len(sys.argv)>2 else None
examples=Path(__file__).resolve().parents[1]/'examples'
with tempfile.TemporaryDirectory(prefix='basic-h-examples-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable,root)
    try:
        for name,exit_key in [('PONG',0x8c),('GRAPHICS',0x88)]:
            s.command('NEW')
            lines=(examples/(name+'.bas')).read_text().splitlines()
            for line in lines:
                assert len(line)<=80,(name,line)
                s.line(line)
            s.command('LIST','\n'.join(lines))
            s.command(f'SAVE "{name}"')
            compiled=Path(root,'T:','basic',name)
            reference=examples/(name+'.prg')
            if output:
                output.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(compiled,output/(name+'.prg'))
            else:
                assert compiled.read_bytes()==reference.read_bytes(),f'{name}: regenerate tokenized example'
            s.command('NEW');s.command(f'LOAD "{name}"')
            s.command('LIST','\n'.join(lines))
            start=len(s.trace.read_text().splitlines()) if s.trace.exists() else 0
            s.send(b'RUN\n');s.receive(b'RUN\n')
            time.sleep(.15)
            s.send(bytes([0x80])) # held LEFT, then release; native sprite movement sample
            time.sleep(.1)
            s.send(bytes([0x81,0x84])) # UP moves player paddle
            time.sleep(.1)
            s.send(bytes([0x85,exit_key]))
            result=s.receive()
            assert b'ERROR' not in result,result
            trace=s.trace.read_text().splitlines()[start:]
            frames=[line for line in trace if line.startswith('FRAME ')]
            assert len(frames)>=3 and len(set(frames))>=3,(name,frames)
            assert trace[-1]=='CLOSE 0'
            s.send(bytes([exit_key+1]))
            s.command('PRINT 123','123')
            print(name,'LIST/SAVE/LOAD/animation/input/TEXT passed:',len(frames),'frames')
        for name in ['C64SCREEN','C64COLORS','C64SPRITE','C64ANIM','J0SPRITE','J0BORDER','J0CELL','J0FULL']:
            s.command('NEW')
            lines=(examples/(name+'.bas')).read_text().splitlines()
            for line in lines:
                assert len(line)<=80,(name,line)
                s.line(line)
            s.command('LIST','\n'.join(lines))
            s.command(f'SAVE "{name}"')
            compiled=Path(root,'T:','basic',name)
            reference=examples/(name+'.prg')
            if output:
                output.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(compiled,output/(name+'.prg'))
            else:
                assert compiled.read_bytes()==reference.read_bytes(),f'{name}: regenerate tokenized example'
            s.command('NEW');s.command(f'LOAD "{name}"')
            s.command('LIST','\n'.join(lines))
            start=len(s.trace.read_text().splitlines()) if s.trace.exists() else 0
            if name=='C64ANIM':
                s.send(b'RUN\n');s.receive(b'RUN\n');time.sleep(.15);s.send(b'\x03')
                result=s.receive()
                assert b'BREAK IN 50' in result or b'BREAK IN 60' in result,result
            else:
                s.command('RUN')
                if not name.startswith('J0'):
                    s.command('TEXT')
            trace=s.trace.read_text().splitlines()[start:]
            frames=[line for line in trace if line.startswith('FRAME ')]
            assert frames,(name,trace)
            assert trace[-1]=='CLOSE 0',(name,trace[-10:])
            s.command('PRINT 123','123')
            print(name,'LIST/SAVE/LOAD/render/TEXT-or-break passed:',len(frames),'frames')
        for name in ['SIDTONE','SIDSCALE','SIDENV','SID3VOICE','SIDSPRITE']:
            s.command('NEW')
            lines=(examples/(name+'.bas')).read_text().splitlines()
            for line in lines:
                assert len(line)<=80,(name,line)
                s.line(line)
            s.command('LIST','\n'.join(lines))
            s.command(f'SAVE "{name}"')
            compiled=Path(root,'T:','basic',name)
            reference=examples/(name+'.prg')
            if output:
                output.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(compiled,output/(name+'.prg'))
            else:
                assert compiled.read_bytes()==reference.read_bytes(),f'{name}: regenerate tokenized example'
            s.command('NEW');s.command(f'LOAD "{name}"')
            s.command('LIST','\n'.join(lines))
            start=len(s.trace.read_text().splitlines()) if s.trace.exists() else 0
            if name=='SIDSPRITE':
                s.send(b'RUN\n');s.receive(b'RUN\n');time.sleep(.12);s.send(b'\x03')
                result=s.receive()
                assert b'BREAK IN 70' in result or b'BREAK IN 80' in result,result
                s.command('TEXT')
            else:
                s.command('RUN')
                time.sleep(.02)
                s.command('TEXT')
            trace=s.trace.read_text().splitlines()[start:]
            assert 'AUDIO_OPEN 0' in trace and 'AUDIO_CLOSE 0' in trace,(name,trace)
            assert any(line.startswith('PCM ') for line in trace),(name,trace)
            s.command('PRINT 123','123')
            print(name,'LIST/SAVE/LOAD/PCM/cleanup passed')
        name='AMAZING'
        s.command('NEW')
        lines=(examples/(name+'.bas')).read_text().splitlines()
        for line in lines:
            assert len(line)<=80,(name,line)
            s.line(line)
        s.command('LIST','\n'.join(lines))
        s.command(f'SAVE "{name}"')
        compiled=Path(root,'T:','basic',name)
        reference=examples/(name+'.prg')
        if output:
            output.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(compiled,output/(name+'.prg'))
        else:
            assert compiled.read_bytes()==reference.read_bytes(),f'{name}: regenerate tokenized example'
        s.command('NEW');s.command(f'LOAD "{name}"')
        s.send(b'RUN\n');s.receive(b'WHAT ARE YOUR WIDTH AND LENGTH? ')
        s.send(b'4,3\n');result=s.receive()
        assert b'.--' in result and b':--' in result and b'ERROR' not in result,result
        print(name,'unchanged LIST/SAVE/LOAD/maze generation passed')
        name='C64DODGE'
        s.command('NEW')
        lines=(examples/(name+'.bas')).read_text().splitlines()
        for line in lines:
            assert len(line)<=80,(name,line)
            s.line(line)
        s.command('LIST','\n'.join(lines))
        s.command(f'SAVE "{name}"')
        compiled=Path(root,'T:','basic',name)
        reference=examples/(name+'.prg')
        if output:
            output.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(compiled,output/(name+'.prg'))
        else:
            assert compiled.read_bytes()==reference.read_bytes(),f'{name}: regenerate tokenized example'
        s.command('NEW');s.command(f'LOAD "{name}"')
        start=len(s.trace.read_text().splitlines()) if s.trace.exists() else 0
        s.send(b'RUN\n');s.receive(b'PRESS ENTER TO START? ');s.send(b'\n')
        time.sleep(.15);s.send(bytes([0x80]));time.sleep(.05);s.send(bytes([0x81]))
        s.send(bytes([0x82]));time.sleep(.05);s.send(bytes([0x83,0x8c]))
        result=s.receive();s.send(bytes([0x8d]))
        assert b'SCORE' in result and b'ERROR' not in result,result
        trace=s.trace.read_text().splitlines()[start:]
        frames=[line for line in trace if line.startswith('FRAME ')]
        assert len(frames)>=3 and len(set(frames))>=3,(name,frames)
        assert 'AUDIO_OPEN 0' in trace and 'AUDIO_CLOSE 0' in trace,(name,trace)
        assert trace[-1]=='CLOSE 0',(name,trace[-10:])
        s.command('PRINT 123','123')
        print(name,'LIST/SAVE/LOAD/controls/graphics/SID/TEXT passed:',len(frames),'frames')
        s.command(f'LOAD "{name}"')
        s.line('100 X=140:OY=21:OX=17:S=0')
        s.send(b'RUN\n');s.receive(b'PRESS ENTER TO START? ');s.send(b'\n')
        result=s.receive()
        assert b'GAME OVER. SCORE 0' in result and b'ERROR' not in result,result
        s.command(f'LOAD "{name}"')
        s.line('100 X=140:OY=21:OX=4:S=0')
        s.line('240 TEXT:PRINT S:END')
        s.send(b'RUN\n');s.receive(b'PRESS ENTER TO START? ');s.send(b'\n')
        result=s.receive()
        assert b' 1 ' in result and b'ERROR' not in result,result
        print(name,'deterministic collision and scoring cases passed')
        s.command('LOAD "PONG"')
        s.line('290 SPRITEPOS 0,X,Y:PRESENT:GOTO 300')
        cases = [
            ('160: Y=26:V=3:W=-1','Y=26 AND W=1'),
            ('160:Y=186:V=3:W=1','Y=186 AND W=-1'),
            ('19:Y=100:V=-3:W=1','X=18 AND V>3'),
            ('297:Y=100:V=3:W=1','X=298 AND V<-3'),
            ('1:Y=150:V=-3:W=1','C=1 AND P=0 AND X=160'),
            ('315:Y=150:V=3:W=1','P=1 AND C=0 AND X=160'),
        ]
        for initial, condition in cases:
            s.line('40 P=0:C=0:PY=90:CY=90:K=0:X='+initial)
            s.line('300 TEXT:PRINT '+condition+':END')
            s.command('RUN','-1')
        print('PONG: six deterministic wall/paddle/scoring cases passed')
    finally:s.close()
