#!/usr/bin/env python3
"""Bounded virtual SID register, synthesis, lifecycle and safety checks."""

from pathlib import Path
import sys
import tempfile
import time

from session import Session

executable = str(Path(sys.argv[1]).resolve())
checks = 0

def trace_lines(session):
    return session.trace.read_text().splitlines() if session.trace.exists() else []

with tempfile.TemporaryDirectory(prefix='basic-j1-sid-') as root:
    Path(root, 'T:').mkdir()
    session = Session(executable, root)
    try:
        session.command('PRINT PEEK(54272);PEEK(54296)', '0  0')
        for address, value, expected in [
            (54272, 214, 214), (54273, 28, 28), (54274, 255, 255),
            (54275, 255, 15), (54276, 16, 16), (54277, 17, 17),
            (54278, 240, 240), (54296, 255, 255),
        ]:
            session.command(f'POKE {address},{value}:PRINT PEEK({address})', str(expected))
        session.command('POKE 54296,15:POKE 54276,17')
        time.sleep(.04)
        lines = trace_lines(session)
        assert lines.count('AUDIO_OPEN 0') == 1, lines
        hashes = [line for line in lines if line.startswith('PCM_HASH ')]
        assert hashes and len(set(hashes)) > 1, hashes
        session.command('POKE 54276,16')
        time.sleep(.05)
        session.command('PRINT 123', '123')
        assert trace_lines(session)[-1] == 'AUDIO_CLOSE 0', trace_lines(session)[-10:]
        checks += 5

        # TEXT is an explicit compatibility-audio cleanup boundary.
        session.command('POKE 54276,17')
        time.sleep(.02)
        session.command('TEXT')
        assert trace_lines(session)[-1] == 'AUDIO_CLOSE 0'
        session.command('PRINT PEEK(54276)', '16')
        checks += 2

        # Programs with SID POKEs retain ordinary LIST/SAVE/LOAD behavior.
        session.command('NEW')
        listing = '10 POKE 54296,15:POKE 54272,214:POKE 54273,28\n20 POKE 54276,17\n30 POKE 54276,16'
        for line in listing.splitlines():
            session.line(line)
        session.command('LIST', listing)
        session.command('SAVE "SIDTEST"')
        session.command('NEW')
        session.command('LOAD "SIDTEST"')
        session.command('LIST', listing)
        checks += 2

        # Ctrl+C stops synthesis and leaves the stored program available.
        session.command('NEW')
        for line in ['10 POKE 54296,15:POKE 54272,214:POKE 54273,28',
                     '20 POKE 54276,33', '30 GOTO 30']:
            session.line(line)
        session.send(b'RUN\n')
        session.receive(b'RUN\n')
        time.sleep(.03)
        session.send(b'\x03')
        result = session.receive()
        assert b'BREAK IN 30' in result, result
        assert trace_lines(session)[-1] == 'AUDIO_CLOSE 0'
        session.command('LIST', '\n'.join(['10 POKE 54296,15:POKE 54272,214:POKE 54273,28',
                                            '20 POKE 54276,33', '30 GOTO 30']))
        checks += 3
    finally:
        checks += session.checks
        session.close()

# Each supported waveform produces deterministic, non-silent PCM through the
# public transport. A fresh process makes the envelope/phase seed repeatable.
waveform_hashes = {}
for name, control in [('triangle', 17), ('saw', 33), ('pulse', 65), ('noise', 129)]:
    with tempfile.TemporaryDirectory(prefix=f'basic-j1-{name}-') as root:
        Path(root, 'T:').mkdir()
        session = Session(executable, root)
        try:
            session.command('POKE 54296,15:POKE 54277,0:POKE 54278,240')
            session.command('POKE 54272,214:POKE 54273,28:POKE 54274,0:POKE 54275,8')
            session.command(f'POKE 54276,{control}')
            time.sleep(.03)
            hashes = [line for line in trace_lines(session) if line.startswith('PCM_HASH ')]
            assert hashes, (name, trace_lines(session))
            waveform_hashes[name] = hashes[0]
            session.command('TEXT')
            checks += 2
        finally:
            checks += session.checks
            session.close()

assert len(set(waveform_hashes.values())) == len(waveform_hashes), waveform_hashes
assert waveform_hashes == {
    'triangle': 'PCM_HASH 2100951947',
    'saw': 'PCM_HASH 1948953137',
    'pulse': 'PCM_HASH 2020433028',
    'noise': 'PCM_HASH 3362668907',
}, waveform_hashes
checks += len(waveform_hashes)

def first_peak(volume, attack):
    with tempfile.TemporaryDirectory(prefix='basic-j1-level-') as root:
        Path(root, 'T:').mkdir()
        session = Session(executable, root)
        try:
            session.command(f'POKE 54296,{volume}:POKE 54277,{attack*16}:POKE 54278,240')
            session.command('POKE 54272,214:POKE 54273,28:POKE 54276,17')
            time.sleep(.02)
            peaks = [int(line.split()[1]) for line in trace_lines(session) if line.startswith('PCM_PEAK ')]
            assert peaks, trace_lines(session)
            session.command('TEXT')
            return peaks[0]
        finally:
            session.close()

full_peak = first_peak(15, 0)
half_peak = first_peak(7, 0)
slow_attack_peak = first_peak(15, 15)
assert full_peak > half_peak > 0, (full_peak, half_peak)
assert full_peak > slow_attack_peak >= 0, (full_peak, slow_attack_peak)
checks += 3
print(f'{checks} virtual SID register, waveform, PCM, lifecycle, storage and break checks passed')
