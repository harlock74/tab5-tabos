#!/usr/bin/env python3
"""Production core/adapters + public SDK, deterministic canvas/PCM transport."""
from pathlib import Path
import sys
import tempfile
import time

executable = str(Path(sys.argv[1]).resolve())
checks = 0
from session import Session

with tempfile.TemporaryDirectory(prefix='basic-h-graphics-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable, root)
    try:
        for key in ['LEFT','RIGHT','UP','DOWN','SPACE','A','B']:
            s.command(f'PRINT KEY("{key}")','0')
        for i,key in enumerate(['LEFT','RIGHT','UP','DOWN','SPACE','A','B']):
            s.send(bytes([0x80+2*i]))
            s.command(f'PRINT 2+KEY("{key.lower()}")','1')
            s.send(bytes([0x81+2*i]))
            s.command(f'PRINT KEY("{key}")','0')
        s.command('PRINT KEY(LEFT$("LEFT",4))','0')
        s.command('PRINT KEY("LEFT")+KEY("RIGHT")','0')
        for cmd,err in [('PRINT KEY("INVALID")','ILLEGAL QUANTITY'),('PRINT KEY(1)','TYPE MISMATCH'),
                        ('KEY "LEFT"','SYNTAX'),('PRINT GRAPHICS','SYNTAX'),('COLOR "A"','TYPE MISMATCH'),
                        ('SLEEP -10','ILLEGAL QUANTITY'),('SOUND 0,100','ILLEGAL QUANTITY')]:
            s.command(cmd,f'?{err}  ERROR')
            s.command('PRINT 123','123')
        s.command('GRAPHICS')
        s.command('COLOR 2:PSET 10,20:PRESENT')
        red=((220&248)<<8)|((60&252)<<3)|(60>>3)
        assert s.pixel(10,20)==red and s.pixel(11,20)==0
        s.command('CLS:COLOR 1:LINE -5,0,5,0:PRESENT')
        assert all(s.pixel(x,0)==65535 for x in range(6)) and s.pixel(6,0)==0
        s.command('CLS:RECT 10,10,5,4:PRESENT')
        assert s.pixel(10,10)==65535 and s.pixel(14,13)==65535 and s.pixel(11,11)==0
        s.command('CLS:RECT 10,10,5,4,1:PRESENT')
        assert s.pixel(11,11)==65535 and s.pixel(15,13)==0
        s.command('CLS:CIRCLE 20,20,3:PRESENT')
        assert all(s.pixel(x,y)==65535 for x,y in [(20,17),(20,23),(17,20),(23,20)])
        assert s.pixel(20,20)==0
        s.command('CLS:CIRCLE 20,20,3,1:PRESENT')
        assert s.pixel(20,20)==65535
        s.command('CLS:CIRCLE 0,0,0:PRESENT')
        assert s.pixel(0,0)==65535
        s.command('CLS:SPRITE 0,3,2:SPRITEROW 0,0,".12"')
        s.command('SPRITEROW 0,1,"345":SPRITEPOS 0,10,20:SPRITESHOW 0:PRESENT')
        assert s.pixel(10,20)==0 and s.pixel(11,20)==65535 and s.pixel(12,20)==red
        s.command('SPRITE 1,1,1:SPRITEROW 1,0,"2":SPRITEPOS 1,11,20')
        s.command('SPRITESHOW 1:PRESENT')
        assert s.pixel(11,20)==red
        s.command('SPRITEPOS 0,-1,-1:SPRITEHIDE 1:PRESENT')
        assert s.pixel(11,20)==0 and s.pixel(0,0)!=0
        s.command('SPRITEHIDE 0:PRESENT')
        assert not any(s.pixels.read_bytes())
        s.command('GTEXT 0,0,"PONG 123":PRESENT')
        assert any(s.pixels.read_bytes())
        for cmd in ['COLOR -1','PSET -999,99999','CIRCLE 10,10,-1','SPRITE 999,0,0,0',
                    'RECT 0,0,-1,5','RECT 0,0,5,5,2','SPRITE 16,1,1','SPRITEROW 0,99,"A"',
                    'SPRITE 0,17,1','CIRCLE 0,0,513','LINE 0,0,32767,0','PSET 1,2,3']:
            s.command('GRAPHICS')
            s.send(cmd.encode()+b'\n')
            result=s.receive()
            assert b'ERROR' in result,(cmd,result)
            assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
            s.command('PRINT 123','123')
        s.command('NEW')
        for line in ['10 graphics:cls:color 5','20 rect 0,0,10,10,1:present',
                     '30 if 1 then color 2:pset 1,1:present',
                     '40 text:print "MiXeD GRAPHICS"',
                     '50 rem GRAPHICS COLOR KEY preserve',
                     '60 data GRAPHICS,"MiXeD":read a$,b$:print a$;b$']:
            s.line(line)
        listing='10 GRAPHICS:CLS:COLOR 5\n20 RECT 0,0,10,10,1:PRESENT\n30 IF 1 THEN COLOR 2:PSET 1,1:PRESENT\n40 TEXT:PRINT "MiXeD GRAPHICS"\n50 REM GRAPHICS COLOR KEY preserve\n60 DATA GRAPHICS,"MiXeD":READ A$,B$:PRINT A$;B$'
        s.command('LIST',listing)
        s.command('SAVE "GRAPHICS"')
        s.command('NEW');s.command('LOAD "GRAPHICS"')
        s.command('LIST',listing)
        s.command('RUN','MiXeD GRAPHICS\nGRAPHICSMiXeD')
        for i in range(5):
            s.command('NEW')
            s.line('10 GRAPHICS:CIRCLE 160,100,512,1:SLEEP 5000:GOTO 10')
            s.send(b'RUN\n');s.receive(b'RUN\n')
            time.sleep(.03)
            begin=time.monotonic();s.send(b'\x03')
            result=s.receive()
            assert b'BREAK IN 10' in result,result
            assert time.monotonic()-begin<1
            assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
            s.command('LIST','10 GRAPHICS:CIRCLE 160,100,512,1:SLEEP 5000:GOTO 10')
            s.command('GRAPHICS');s.command('TEXT');s.command('PRINT 123','123')
        s.command('SOUND 440,20')
        trace=s.trace.read_text().splitlines()
        assert trace[-1]=='AUDIO_CLOSE 0'
        assert sum(int(v.split()[1]) for v in trace if v.startswith('PCM '))==1764
        # Held-state reads leave the GET character queue untouched.
        s.command('NEW')
        s.line('10 GET A$:IF A$="" THEN 10')
        s.line('20 PRINT A$;KEY("LEFT")')
        s.send(b'RUN\n');s.receive(b'RUN\n');s.send(b'\x80Z')
        assert b'Z-1' in s.receive()
        s.send(b'\x81')
        s.command('GRAPHICS')
    finally:
        checks += s.checks
        s.close()
    assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
    for env in [{'TABOS_BASIC_GRAPHICS_UNAVAILABLE':'1'},{'TABOS_BASIC_AUDIO_UNAVAILABLE':'1'}]:
        s=Session(executable, root,**env)
        try:
            if 'TABOS_BASIC_GRAPHICS_UNAVAILABLE' in env:
                s.command('GRAPHICS','?ILLEGAL QUANTITY  ERROR')
            else:
                s.command('SOUND 440,20','?TABOS AUDIO UNAVAILABLE\n\n?ILLEGAL QUANTITY  ERROR')
            s.command('PRINT 123','123')
        finally:
            checks += s.checks
            s.close()
# A physical B also has a cooked text event. Preserve it for BASIC, but do
# not leave the parent's prompt attached to the child's unfinished edit line.
with tempfile.TemporaryDirectory(prefix='basic-h-exit-echo-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable,root)
    try:
        s.line('10 GRAPHICS:IF KEY("B")=0 THEN 10')
        s.line('20 TEXT:END')
        s.send(b'RUN\n');s.receive(b'RUN\n')
        s.send(b'\x8cb')
        ending=s.receive()
        s.send(b'\x8d')
        time.sleep(.03)
        s.send(b'\x11');assert s.p.wait(timeout=3)==0
        ending+=s.p.stdout.read()
        assert ending.endswith(b'READY.\nb\n'),ending
    finally:s.close()

print(f'{checks} graphics/language commands plus pixel, held-key, PCM and five break/transition checks passed')
