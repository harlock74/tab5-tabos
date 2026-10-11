#!/usr/bin/env python3
"""Wall-time observations, not frame-rate guarantees or performance assertions."""
from pathlib import Path
import sys
import tempfile
import time
from session import Session

with tempfile.TemporaryDirectory(prefix='basic-h-bench-') as root:
    Path(root,'T:').mkdir()
    s=Session(str(Path(sys.argv[1]).resolve()),root)
    try:
        s.command('GRAPHICS:SPRITE 0,16,16')
        for row in range(16):
            s.command(f'SPRITEROW 0,{row},"123456789ABCDEF1"')
        s.command('SPRITESHOW 0')
        for line in Path(__file__).with_suffix('.tsv').read_text().splitlines():
            name, iterations, statement=line.split('\t')
            s.command('NEW')
            s.line(f'10 FOR I=1 TO {iterations}:{statement}:NEXT')
            s.line('20 END')
            start=time.perf_counter()
            s.command('RUN')
            elapsed=(time.perf_counter()-start)*1000
            print(f'{name}\t{iterations}\t{elapsed:.3f} ms',flush=True)
        s.command('TEXT')
    finally:s.close()
