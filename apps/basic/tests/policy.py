#!/usr/bin/env python3
"""Stage G policy regressions with Stage H0 clock/cursor repair expectations."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='tabos-basic-policy-') as root, tempfile.TemporaryFile() as errors:
        clock = Path(root, 'clock')
        milliseconds = 1_000_000
        clock.write_text(str(milliseconds))
        env = dict(os.environ, TABOS_BASIC_TEST_CLOCK=str(clock))
        process = subprocess.Popen([str(executable)], cwd=root, env=env,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors)
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        pending = b''
        checks = 0
        failures = []

        def receive(marker=b'READY.\n'):
            nonlocal pending
            deadline = time.monotonic() + 10
            while marker not in pending:
                assert time.monotonic() < deadline, (marker, pending)
                for key, _ in selector.select(.1):
                    chunk = os.read(key.fd, 8192)
                    assert chunk, ('unexpected exit', pending)
                    pending += chunk
            end = pending.index(marker) + len(marker)
            result, pending = pending[:end], pending[end:]
            return result

        def advance(delta):
            nonlocal milliseconds
            assert delta >= 0
            milliseconds += delta
            replacement = clock.with_suffix('.next')
            replacement.write_text(str(milliseconds))
            replacement.replace(clock)

        def check(command, expected):
            nonlocal checks
            wire = command.encode('ascii') + b'\n'
            process.stdin.write(wire)
            process.stdin.flush()
            assert receive(b'\n') == wire
            if expected is None:
                checks += 1
                return
            actual = receive()[:-len(b'READY.\n')].decode('ascii').strip()
            checks += 1
            print(repr(command), '=>', repr(actual), flush=True)
            if actual != expected:
                failures.append((command, expected, actual))

        try:
            receive()
            check('PRINT TI;TI$', '0 000000')
            advance(16)
            check('PRINT TI', '0')
            advance(1)
            check('PRINT TI', '1')
            advance(983)
            check('PRINT TI;TI$', '60 000001')
            advance(60_000)
            check('PRINT TI;TI$', '3660 000101')
            check('TI=100', '?SYNTAX  ERROR')
            check('PRINT TI;TI$', '3660 000101')
            advance(86_400_000 - 61_000 - 1)
            check('PRINT TI;TI$', '5183999 235959')
            advance(1)
            check('PRINT TI;TI$', '0 000000')
            advance(86_400_000 + 1000)
            check('PRINT TI;TI$', '60 000001')
            check('NEW', '')
            check('CLR', '')
            check('RUN', '')
            check('PRINT TI;TI$', '60 000001')
            for command, expected in [
                ('TI$="12345"', '?ILLEGAL QUANTITY  ERROR'),
                ('TI$="1234567"', '?ILLEGAL QUANTITY  ERROR'),
                ('TI$=123456', '?TYPE MISMATCH  ERROR'),
                ('PRINT POS(0)', '0'), ('PRINT "ABC";POS(0)', 'ABC 3'),
                ('PRINT "A";TAB(5);"B"', 'A    B'),
                ('PRINT "A","B"', 'A         B'),
                ('PRINT ST', '0'),
                ('PRINT#1,"X"', '?UNSUPPORTED IN TABOS STAGE B\n\n?ILLEGAL QUANTITY  ERROR'),
                ('INPUT#1,A', '?UNSUPPORTED IN TABOS STAGE B\n\n?ILLEGAL QUANTITY  ERROR'),
                ('GET#1,A$', '?ILLEGAL DIRECT  ERROR'),
                ('10 GET#1,A$', None),
                ('RUN', '?UNSUPPORTED IN TABOS STAGE B\n\n?ILLEGAL QUANTITY  ERROR IN 10'),
                ('NEW', ''),
                ('PRINT "A";CHR$(147);"B"', 'AB'),
                ('PRINT "A";CHR$(255);"B"', 'AB'),
            ]:
                check(command, expected)
            for value, ticks in [('000000', 0), ('123456', 2717760), ('235959', 5183940)]:
                check(f'TI$="{value}"', '')
                check('PRINT TI;TI$', f'{ticks} {value}')
            advance(1000)
            check('PRINT TI;TI$', '0 000000')
            check('TI$="123456"', '')
            for value in ['240000', '126000', '123460', 'ABCDEF', '12 456']:
                check(f'TI$="{value}"', '?ILLEGAL QUANTITY  ERROR')
                check('PRINT TI;TI$', '2717760 123456')
            check('PRINT "1234567890";TAB(5);"X"', '1234567890X')
            check('PRINT "1234567890","X"', '1234567890          X')
            check('A$="'+ 'X'*40 +'"', '')
            check('PRINT A$;A$;POS(0)', 'X'*80+' 0')
            check('PRINT "ABC";:PRINT POS(0)', 'ABC 3')
            check('A$="123456":TI$=A$', '')
            check('PRINT TI$', '123456')
            check('10 TI$="000000":PRINT TI$', None)
            check('RUN', '000000')
            check('10 TI$="126000"', None)
            check('RUN', '?ILLEGAL QUANTITY  ERROR IN 10')
            check('LIST', '10 TI$="126000"')
            check('NEW', '')
            check('PRINT 1/0', '?DIVISION BY ZERO  ERROR')
            check('PRINT POS(0)', '0')
            process.stdin.write(b'\x11')
            process.stdin.flush()
            assert process.wait(timeout=10) == 0
            errors.seek(0)
            diagnostics = errors.read()
            assert not diagnostics, diagnostics
            assert not failures, failures
            print(f'{checks} deterministic read/error/output checks passed')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            errors.seek(0)
            diagnostics = errors.read()
            if diagnostics:
                print(diagnostics.decode('utf-8', errors='replace'), file=sys.stderr)
            selector.close()
            process.stdin.close()
            process.stdout.close()



if __name__ == '__main__':
    main()
