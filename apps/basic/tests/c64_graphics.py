#!/usr/bin/env python3
"""Safe virtual-C64 PEEK/POKE, renderer, lifecycle and security checks."""
from pathlib import Path
import re
import sys
import tempfile
import time

from session import Session

executable = str(Path(sys.argv[1]).resolve())
checks = 0

def rgb565(red, green, blue):
    return ((red & 248) << 8) | ((green & 252) << 3) | (blue >> 3)

palette = [
    rgb565(0,0,0), rgb565(255,255,255), rgb565(136,0,0), rgb565(170,255,238),
    rgb565(204,68,204), rgb565(0,204,85), rgb565(0,0,170), rgb565(238,238,119),
    rgb565(221,136,85), rgb565(102,68,0), rgb565(255,119,119), rgb565(51,51,51),
    rgb565(119,119,119), rgb565(170,255,102), rgb565(0,136,255), rgb565(187,187,187),
]

with tempfile.TemporaryDirectory(prefix='basic-i-c64-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable,root)
    try:
        s.command('PRINT PEEK(1024)','32')
        for address,value in [(0,17),(1024,65),(2023,66),(2040,13),(2047,14),(16383,255),
                              (55296,2),(56295,31)]:
            s.command(f'POKE {address},{value}:PRINT PEEK({address})',str(value if address<55296 else value&15))
        for address in list(range(53248,53265))+[53269,53271,53276,53277,53280,53281,53285,53286]+list(range(53287,53295)):
            value=address&255
            expected=value&15 if address in [53280,53281,53285,53286] or address>=53287 else value
            s.command(f'POKE {address},{value}:PRINT PEEK({address})',str(expected))
        s.command('POKE 53264,0:POKE 53269,0:POKE 53271,0:POKE 53276,0:POKE 53277,0')
        s.command('POKE 53280,14:POKE 53281,6')
        s.command('POKE 1024,65:POKE 55296,2')
        s.command('PRINT PEEK(1024);PEEK(55296)','65  2')
        assert s.pixel(0,0)==palette[14]
        assert s.pixel(20,12)==palette[6]
        assert s.pixel(22,12)==palette[2]
        checks += 3

        for color in range(16):
            s.command(f'POKE 53280,{color}:POKE 53281,{15-color}')
            s.command('PRINT PEEK(53280);PEEK(53281)',f'{color}  {15-color}')
            assert s.pixel(0,0)==palette[color]
            assert s.pixel(20,12)==palette[15-color]
            checks += 2
        s.command('POKE 55296,31:PRINT PEEK(55296)','15')

        # Pointer 13 selects bytes 832..895 in the fixed virtual VIC bank.
        s.command('FOR I=832 TO 894:POKE I,0:NEXT')
        s.command('POKE 832,255:POKE 2040,13')
        s.command('POKE 53248,100:POKE 53249,80:POKE 53287,5:POKE 53269,1')
        assert s.pixel(100,80)==palette[5]
        assert s.pixel(108,80)!=palette[5]
        s.command('POKE 53277,1:POKE 53271,1')
        assert s.pixel(115,81)==palette[5]
        checks += 3

        # Incremental sprite restoration preserves overlapping lower-priority
        # sprites and removes both normal and expanded old bounds without trails.
        s.command('POKE 2041,13:POKE 53250,104:POKE 53251,80:POKE 53288,2:POKE 53269,3')
        assert s.pixel(104,80)==palette[5]
        s.command('POKE 53248,120')
        assert s.pixel(100,80)==palette[0] and s.pixel(104,80)==palette[2]
        assert s.pixel(120,80)==palette[5]
        s.command('POKE 53269,1')
        assert s.pixel(104,80)==palette[0]
        s.command('POKE 53277,1:POKE 53248,140')
        assert s.pixel(139,80)==palette[0]
        assert s.pixel(140,80)==palette[5] and s.pixel(155,80)==palette[5]
        s.command('POKE 53277,0:POKE 53248,100')
        checks += 7

        # Multicolour pair 01 uses D025 and occupies two logical pixels.
        s.command('POKE 53277,0:POKE 53271,0:POKE 53276,1')
        s.command('POKE 53285,3:POKE 53286,4:POKE 832,64')
        assert s.pixel(100,80)==palette[3] and s.pixel(101,80)==palette[3]
        checks += 1
        s.command('POKE 53264,1:POKE 53248,255')
        assert s.pixel(319,80)!=palette[3]
        s.command('POKE 53264,0:POKE 53248,100')
        checks += 1

        for command in ['PRINT PEEK(16384)','PRINT PEEK(49152)','PRINT PEEK(54297)',
                        'POKE 16384,1','POKE 49152,1','POKE 54297,1']:
            s.command(command,'?UNSUPPORTED C64 ADDRESS $'+format(int(command.split('(')[1].split(')')[0]) if 'PEEK' in command else int(command.split()[1].split(',')[0]),'04X')+'\n\n?ILLEGAL QUANTITY  ERROR')
        for command in ['PRINT PEEK(-1)','POKE -1,1','POKE 70000,1','POKE 53280,-1','POKE 53280,256']:
            s.command(command,'?ILLEGAL QUANTITY  ERROR')
        for command in ['SYS 49152','PRINT USR(0)','WAIT 53280,1']:
            s.command(command,'?UNSUPPORTED IN TABOS STAGE B\n\n?ILLEGAL QUANTITY  ERROR')
        s.command('A=7:POKE 0,255:POKE 768,0:POKE 784,0:PRINT A','7')
        s.command('PRINT PEEK(0);PEEK(768);PEEK(784)','255  0  0')

        # Native and compatibility renderers explicitly hand off one canvas.
        s.command('GRAPHICS:CLS:COLOR 1:PSET 1,1:PRESENT')
        assert s.pixel(1,1)==palette[1]
        s.command('POKE 53280,2')
        assert s.pixel(0,0)==palette[2]
        s.command('TEXT')
        assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
        checks += 3
        for color in [3,5,7]:
            s.command('GRAPHICS')
            s.command(f'POKE 53280,{color}')
            assert s.pixel(0,0)==palette[color]
            s.command('TEXT')
            assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
            checks += 2

        s.command('NEW')
        for line in ['10 FOR I=0 TO 999','20 POKE 1024+I,65','30 POKE 55296+I,I AND 15','40 NEXT I']:
            s.line(line)
        listing='10 FOR I=0 TO 999\n20 POKE 1024+I,65\n30 POKE 55296+I,I AND 15\n40 NEXT I'
        s.command('LIST',listing)
        s.command('SAVE "C64FILL"')
        s.command('NEW');s.command('LOAD "C64FILL"');s.command('LIST',listing)
        begin=time.monotonic();s.command('RUN');elapsed=time.monotonic()-begin
        assert elapsed < 8 and s.pixel(22,12)==palette[0]
        print(f'BENCH C64_MIXED_2000 {elapsed*1000:.1f} ms')
        checks += 2

        for name,body in [
            ('C64_SCREEN_1000','POKE 1024+I,65'),
            ('C64_COLOR_1000','POKE 55296+I,I AND 15'),
            ('C64_BACKGROUND_1000','POKE 53281,I AND 15'),
            ('C64_SPRITE_MOVE_1000','POKE 53248,I AND 255'),
        ]:
            s.command('NEW');s.line(f'10 FOR I=0 TO 999:{body}:NEXT')
            begin=time.monotonic();s.command('RUN');elapsed=time.monotonic()-begin
            print(f'BENCH {name} {elapsed*1000:.1f} ms')
            checks += 1
        begin=time.monotonic();s.command('POKE 53280,0');elapsed=time.monotonic()-begin
        print(f'BENCH C64_FULL_REDRAW {elapsed*1000:.1f} ms')
        checks += 1

        s.command('NEW')
        s.line('10 FOR I=0 TO 999:POKE 1024+I,65:NEXT:GOTO 10')
        s.send(b'RUN\n');s.receive(b'RUN\n')
        time.sleep(.03);begin=time.monotonic();s.send(b'\x03')
        result=s.receive()
        assert b'BREAK IN 10' in result and time.monotonic()-begin<1
        assert s.trace.read_text().splitlines()[-1]=='CLOSE 0'
        s.command('LIST','10 FOR I=0 TO 999:POKE 1024+I,65:NEXT:GOTO 10')
        checks += 3

        # The widely published historical 10 PRINT listing executes unchanged,
        # but its PETSCII maze glyphs are intentionally not claimed by this font.
        s.command('NEW');s.line('10 PRINT CHR$(205.5+RND(1));:GOTO 10')
        s.send(b'RUN\n');s.receive(b'RUN\n');time.sleep(.01);s.send(b'\x03')
        result=s.receive()
        assert b'BREAK IN 10' in result and b'ERROR' not in result
        checks += 1
    finally:
        checks += s.checks
        s.close()

with tempfile.TemporaryDirectory(prefix='basic-i-c64-unavailable-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable,root,TABOS_BASIC_GRAPHICS_UNAVAILABLE='1')
    try:
        s.command('POKE 53280,0','?TABOS C64 GRAPHICS UNAVAILABLE\n\n?ILLEGAL QUANTITY  ERROR')
        s.command('PRINT 123','123')
        checks += s.checks
    finally:s.close()

with tempfile.TemporaryDirectory(prefix='basic-j0-profile-') as root:
    Path(root,'T:').mkdir()
    s=Session(executable,root,args=('--c64-profile',))
    try:
        s.command('POKE 53280,0:POKE 53281,6')
        s.send(b'TEXT\n')
        result=s.receive()
        match=re.search(rb'C64 PROFILE writes=2 redraws=([0-9]+) presents=([0-9]+)[\s\S]*elapsed_ms=([0-9]+)',result)
        assert match and int(match.group(1))>=1 and int(match.group(2))>=1,result
        checks += 4
    finally:s.close()

print(f'{checks} virtual C64 address, pixel, sprite, transition, storage and break checks passed')
