#!/usr/bin/env python3
"""Drive the sanitizer-built production core/adapter over a test SDK boundary."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time


with tempfile.TemporaryDirectory(prefix="tabos-basic-native-") as root, tempfile.TemporaryFile() as errors:
    Path(root, "T:").mkdir()
    process = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=errors, cwd=root)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)

    def receive(marker=b"READY."):
        output = b""
        deadline = time.monotonic() + 10
        while marker not in output:
            assert time.monotonic() < deadline, (marker, output)
            for key, _ in selector.select(0.1):
                chunk = os.read(key.fd, 8192)
                assert chunk, (process.poll(), marker, output)
                output += chunk
        return output

    def send(command):
        process.stdin.write(command.encode("ascii") + b"\n")
        process.stdin.flush()

    def check(command, expected):
        send(command)
        output = receive()
        assert expected in output, (command, output)
        if expected == b"READY." and command.lower().startswith(("save ", "load ")):
            assert b"?" not in output, (command, output)

    try:
        receive()
        check('print "Hello TabOS"', b'\nHello TabOS\n')
        check('print "Hello"', b'\nHello\n')
        for value in ['HELLO', 'A B C', '']:
            check('PRINT "' + value + '"', b'\n' + value.encode() + b'\n')
        check('\b\b\bPRON\b\bINT 2+2', b'\n 4 ')
        printable = ''.join(chr(c) for c in range(32, 127) if c != 34)
        for start in range(0, len(printable), 40):
            value = printable[start:start + 40]
            check('print "' + value + '"', b'\n' + value.encode() + b'\n')
        check('print chr$(34)', b'\n"\n')
        check('a=2:print (a+3)*2/5-1;",";"Case:;() +-*/^=<>!?"',
              b'\n 1 ,Case:;() +-*/^=<>!?\n')
        check("NEW", b"READY.")
        send('10 print "hello"')
        send('20 rem Keep Mixed CASE:print "Not Executed"')
        send('30 data Hello TabOS,"MiXeD:Value":read a$,b$:print a$;":";b$')
        check("list", b'20 REM Keep Mixed CASE:print "Not Executed"')
        check("run", b'\nHello TabOS:MiXeD:Value\n')
        check("new", b"READY.")
        send('10 input a$')
        send('20 print a$')
        send('run')
        receive(b'? ')
        send('MiXeD input rem data')
        assert b'\nMiXeD input rem data\n' in receive()
        send('run')
        receive(b'? ')
        send('MistakX\be')
        assert b'\nMistake\n' in receive()
        send('run')
        receive(b'? ')
        send('')
        empty = receive()
        assert empty == b'\n\n\nREADY.\n', empty
        for draft in [b'', b'unsubmitted Mixed']:
            send('run')
            receive(b'? ')
            process.stdin.write(draft + b'\x03')
            process.stdin.flush()
            interrupted = receive()
            assert b'BREAK IN 10' in interrupted, interrupted
            check('list', b'10 INPUT A$')
            check('print 2+2', b'\n 4 ')
        # Typeahead crosses RUN/STOP polling and two INPUT calls without case loss.
        check('new', b'READY.')
        send('10 input a$,b$:print a$;":";b$')
        process.stdin.write(b'run\nFirst\nSecond\n')
        process.stdin.flush()
        assert b'\nFirst:Second\n' in receive()
        check('new', b'READY.')
        send('10 input a:print a')
        send('run')
        receive(b'? ')
        send('oops')
        receive(b'?REDO FROM START')
        process.stdin.write(b'\x03')
        process.stdin.flush()
        assert b'BREAK IN 10' in receive()
        send('run')
        receive(b'? ')
        send('12.5')
        assert b'\n 12.5 ' in receive()
        check('new', b'READY.')
        send('10 input a,b')
        send('20 print a*2;b*2')
        send('run')
        receive(b'? ')
        send('12X\b,3')
        assert b'\n 24  6 ' in receive()
        send('run')
        receive(b'? ')
        send('')
        empty = receive()
        assert b'\n 0  0 ' in empty, empty
        check('new', b'READY.')
        send('10 a=7:input a:print a')
        send('run')
        receive(b'? ')
        send('')
        assert b'\n 7 ' in receive()
        check('new', b'READY.')
        send('10 a$="OLD":input a$:print "[";a$;"]"')
        send('run')
        receive(b'? ')
        send('')
        assert b'\n[OLD]\n' in receive()
        check('new', b'READY.')
        send('10 input a$:print len(a$)')
        send('run')
        receive(b'? ')
        send('x' * 80)
        assert b'\n 80 ' in receive()
        send('run')
        receive(b'? ')
        send('x' * 81)
        rejected = receive(b'SUBMISSION DISCARDED')
        assert b'READY.' not in rejected, rejected
        send('Replacement')
        assert b'\n 11 ' in receive()
        # Prompt cancellation drops old draft, but subsequent text is preserved.
        process.stdin.write(b'print "DO NOT EXECUTE"\x03print "AFTER BREAK"\n')
        process.stdin.flush()
        result = receive(b'\nAFTER BREAK\n')
        assert b'\nDO NOT EXECUTE\n' not in result, result
        # Complete the trailing READY if it was not read in the same chunk.
        if b'READY.' not in result.split(b'\nAFTER BREAK\n', 1)[1]:
            receive()
        # An exact 80-byte command is safe; byte 81 rejects the whole line.
        valid = 'print "' + 'X' * 72 + '"'
        assert len(valid) == 80
        check(valid, b'\n' + b'X' * 72 + b'\n')
        check(valid + ' ', b'?LINE TOO LONG - SUBMISSION DISCARDED')
        check('print 7', b'\n 7 ')
        # A producer burst exceeding the application queue must not execute a suffix.
        send('X' * 300 + ':print "BAD SUFFIX"')
        result = receive()
        assert b'INPUT OVERRUN' in result, result
        assert b'\nBAD SUFFIX\n' not in result, result
        check('print 8', b'\n 8 ')
        # Ctrl+C must remain usable while rejecting a too-long submission.
        process.stdin.write((valid + ' ').encode())
        process.stdin.flush()
        receive(b'SUBMISSION DISCARDED')
        process.stdin.write(b'\x03')
        process.stdin.flush()
        receive()
        check('print 9', b'\n 9 ')
        process.stdin.write(b'print "BAD"\xc3\xa9\n')
        process.stdin.flush()
        assert b'?NON-ASCII/CONTROL INPUT' in receive()
        check('print 10', b'\n 10 ')
        check("NEW", b"READY.")
        check("PRINT 2+2", b"\n 4 ")
        send('10 PRINT "HELLO TABOS"')
        send("20 END")
        check("RUN", b"\nHELLO TABOS\n")
        check("LIST", b'10 PRINT "HELLO TABOS"')
        check('save "HELLO"', b'READY.')
        saved = Path(root, 'T:/basic/HELLO').read_bytes()
        assert saved[:2] == b'\x01\x08', saved
        check('new', b'READY.')
        check('load "HELLO"', b'READY.')
        check('list', b'10 PRINT "HELLO TABOS"')
        check('run', b'\nHELLO TABOS\n')
        check('save "HELLO",8', b'READY.')
        check('save "T:/basic/Mixed.BAS"', b'READY.')
        assert Path(root, 'T:/basic/Mixed.BAS').read_bytes() == saved
        send('10 PRINT "REPLACED"')
        check('save "HELLO"', b'READY.')
        assert Path(root, 'T:/basic/HELLO').read_bytes() != saved
        check('new', b'READY.')
        check('load "HELLO",8', b'READY.')
        check('run', b'\nREPLACED\n')
        check('load "T:/basic/Mixed.BAS"', b'READY.')
        check('save "' + 'N' * 63 + '"', b'READY.')
        check('load "' + 'N' * 63 + '"', b'READY.')
        check('save "' + 'N' * 64 + '"', b'?INVALID FILENAME')
        # Untrusted files must never replace the live program on failure.
        invalid = [b'', b'\x01', b'\x01\x08\x00', b'\x00\xc0\x00\x00',
                   saved[:-1], saved + b'X', b'\x01\x08' + b'X' * 38912,
                   b'\x01\x08\x01\x08\x0a\x00\x99\x00\x00\x00']
        for payload in invalid:
            Path(root, 'T:/basic/BAD').write_bytes(payload)
            send('load "BAD"')
            assert b'?' in receive()
            check('list', b'10 PRINT "HELLO TABOS"')
        # Longer filename expressions cannot overflow the bounded SETNAM copy.
        check('n$="' + 'X' * 50 + '"', b'READY.')
        check('load n$+n$', b'?INVALID FILENAME')
        for command in ['load "MISSING"', 'load ""', 'save ""',
                        'load "../bad"', 'save "/tmp/bad"', 'load "HELLO",9',
                        'load "HELLO",8,1', 'load "$"']:
            send(command)
            failure = receive()
            assert b'?' in failure, (command, failure)
            check('list', b'10 PRINT "HELLO TABOS"')

        check("NEW", b"READY.")
        for line in ["10 FOR I=1 TO 5", "20 PRINT I", "30 NEXT I", "40 END"]:
            send(line)
        check("RUN", b"\n 1 \n 2 \n 3 \n 4 \n 5 ")
        check("PRINT 2+3\b4", b"\n 6 ")
        for program in ['10 GOTO 10', '10 PRINT "LOOP":GOTO 10',
                        '10 A=SQR(12345)+SIN(1)+COS(2)+LOG(3)+EXP(1):GOTO 10']:
            check("NEW", b"READY.")
            send(program)
            send("RUN")
            time.sleep(0.05)
            started = time.monotonic()
            process.stdin.write(b"\x03")
            process.stdin.flush()
            output = receive()
            assert b"BREAK IN 10" in output, output
            print(f"Native break: {(time.monotonic() - started) * 1000:.1f} ms")
            check("LIST", program.encode())
            check("PRINT 2+2", b"\n 4 ")
        check('new', b'READY.')
        send('10 get a$:if a$="" then 10')
        send('run')
        time.sleep(0.05)
        process.stdin.write(b'\x03')
        process.stdin.flush()
        assert b'BREAK IN 10' in receive()
        check('list', b'10 GET A$:IF A$="" THEN 10')
        check("PRINT PEEK(0)", b"\n 0 ")
        check("POKE 0,0", b"READY.")
        for command in ["SYS 1", "SYS 40960", "PRINT USR(0)", "WAIT 1,1",
                        'VERIFY "X"', 'OPEN 255,8,255,"X"', "CLOSE 255", "CMD 255",
                        "IF 1 THEN SYS 1"]:
            check(command, b"?UNSUPPORTED IN TABOS STAGE B")
            check("PRINT 2+2", b"\n 4 ")
        check("PRINT 1+PEEK(65535)", b"?UNSUPPORTED C64 ADDRESS $FFFF")
        check("PRINT 2+2", b"\n 4 ")
        process.stdin.write(b"\x11")
        process.stdin.flush()
        assert process.wait(timeout=10) == 0
        errors.seek(0)
        diagnostics = errors.read()
        assert not diagnostics, diagnostics
        print("Native production core/input adapter sanitizer checks passed")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        process.stdin.close()
        process.stdout.close()
        selector.close()
