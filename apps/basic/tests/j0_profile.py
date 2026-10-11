#!/usr/bin/env python3
"""Run reproducible Stage J0 C64 profiling workloads."""

from pathlib import Path
import re
import sys
import tempfile

from session import Session

executable = str(Path(sys.argv[1]).resolve())
examples = Path(__file__).resolve().parents[1] / 'examples'
expected_writes = {
    'J0SPRITE': 1112,
    'J0BORDER': 512,
    'J0CELL': 417,
    'J0FULL': 2000,
}
pattern = re.compile(
    rb'C64 PROFILE writes=([0-9]+) redraws=([0-9]+) presents=([0-9]+)'
    rb'\s*C64 PROFILE skipped=([0-9]+) forced=([0-9]+) avg_writes=([0-9]+) '
    rb'max_writes=([0-9]+)\s*C64 PROFILE elapsed_ms=([0-9]+) render_ms=([0-9]+) '
    rb'present_ms=([0-9]+)\s*C64 PROFILE max_render_ms=([0-9]+) max_present_ms=([0-9]+) '
    rb'full=([0-9]+) partial=([0-9]+)'
)

with tempfile.TemporaryDirectory(prefix='basic-j0-profile-') as root:
    Path(root, 'T:').mkdir()
    session = Session(executable, root, args=('--c64-profile',))
    try:
        for name, expected in expected_writes.items():
            session.command('NEW')
            for line in (examples / f'{name}.bas').read_text().splitlines():
                session.line(line)
            session.send(b'RUN\n')
            output = session.receive(timeout=20)
            match = pattern.search(output)
            assert match, (name, output[-2000:])
            values = [int(value) for value in match.groups()]
            assert values[0] == expected, (name, values[0], expected)
            assert values[1] == values[2] and values[1] > 0, (name, values)
            assert values[12] >= 1 and values[12] + values[13] == values[1], (name, values)
            if name == 'J0SPRITE':
                assert values[1] >= 200 and values[6] <= 4, values
            elif name == 'J0CELL':
                assert values[1] >= 40 and values[6] <= 8, values
            elif name == 'J0FULL':
                assert values[1] <= 40, values
            print(name, ' '.join(f'{key}={value}' for key, value in zip(
                ['writes', 'redraws', 'presents', 'skipped', 'forced', 'avg_writes',
                 'max_writes', 'elapsed_ms', 'render_ms', 'present_ms',
                 'max_render_ms', 'max_present_ms', 'full', 'partial'], values)))
    finally:
        session.close()

print('4 Stage J0 profiling workloads passed')
