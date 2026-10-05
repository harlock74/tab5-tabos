#!/usr/bin/env python3
"""Deterministic Stage F transcripts against the production core and adapters.

Each case starts with NEW. Compare complete output (apart from outer whitespace),
not substrings that could accidentally match the echoed command or a failed branch.
Run with the native test executable; --transcript writes reviewable JSON evidence.
"""
import argparse
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import time


CASES = []


def case(name, *steps):
    CASES.append((name, steps))


def expression(name, expression, expected):
    case(name, ('PRINT ' + expression, expected))


def program(name, lines, expected):
    case(name, *((line, None) for line in lines), ('RUN', expected))


for name, value, expected in [
    ('multiply-before-add', '1+2*3', '7'),
    ('parentheses', '(1+2)*3', '9'),
    ('division', '10/4', '2.5'),
    ('power', '2^3', '8'),
    ('unary-minus', '-2^2', '-4'),
    ('negative-base', '(-2)^2', '4'),
    ('fraction', '.25+.5', '.75'),
    ('negative', '-7+2', '-5'),
    ('large', '1E30*10', '1E+31'),
    ('underflow', '1E-39', '0'),
    ('overflow', '1E38*10', '?OVERFLOW  ERROR'),
    ('divide-zero', '1/0', '?DIVISION BY ZERO  ERROR'),
    ('relations', '(2>1);(2=1);(2<>1)', '-1  0 -1'),
    ('logic', '(1 AND 3);(1 OR 2);NOT 0', '1  3 -1'),
]:
    expression('arithmetic.' + name, value, expected)

case('variables.numeric', ('A=10:PRINT A', '10'), ('AB=20:PRINT AB', '20'),
     ('A%=5:PRINT A%', '5'), ('A=2.5:PRINT A', '2.5'), ('PRINT Z', '0'))
case('variables.alias', ('ABC=1:ABD=2:PRINT ABC;ABD', '2  2'))
case('variables.types', ('A=1:A%=2:A$="X":PRINT A;A%;A$', '1  2 X'))
case('variables.integer', ('A%=5.9:PRINT A%', '5'),
     ('A%=-5.9:PRINT A%', '-6'), ('A%=32767:PRINT A%', '32767'),
     ('A%=-32768:PRINT A%', '-32768'), ('A%=32768', '?ILLEGAL QUANTITY  ERROR'))
case('variables.tokenization', ('SCORE=1', '?SYNTAX  ERROR'),
     ('LET AB=3:PRINTAB', '3'))
case('strings.basic', ('A$="TABOS":PRINT A$', 'TABOS'),
     ('B$="":PRINT LEN(B$)', '0'), ('A$=A$+" BASIC":PRINT A$', 'TABOS BASIC'),
     ('A$="Hello TabOS":PRINT A$', 'Hello TabOS'),
     ('PRINT "A:;,+-*/^()!?"', 'A:;,+-*/^()!?'),
     ('PRINT ("A"="A");("A"<"B");("A"="a")', '-1 -1  0'))
case('strings.length', ('A$="X"', ''), ('FOR I=1 TO 7:A$=A$+A$:NEXT', ''),
     ('A$=A$+LEFT$(A$,127):PRINT LEN(A$)', '255'),
     ('A$=A$+"X"', '?STRING TOO LONG  ERROR'))
program('strings.storage', [
    '10 DIM A$(40)', '20 FOR I=0 TO 40:A$(I)=STR$(I):NEXT',
    '30 FOR J=1 TO 100:FOR I=0 TO 40',
    '40 A$(I)=RIGHT$("                    "+STR$(I),20)',
    '50 NEXT I,J', '60 PRINT LEN(A$(0));LEN(A$(40));VAL(A$(40))'], '20  20  40')
for name, value, expected in [
    ('left', 'LEFT$("TABOS",3)', 'TAB'), ('right', 'RIGHT$("TABOS",2)', 'OS'),
    ('mid', 'MID$("TABOS",2,3)', 'ABO'), ('mid-tail', 'MID$("TABOS",4)', 'OS'),
    ('len', 'LEN("TABOS")', '5'), ('chr-asc', 'ASC(CHR$(65))', '65'),
    ('str-val', 'VAL(STR$(-12.5))', '-12.5'), ('val-prefix', 'VAL("12.5XYZ")', '12.5'),
    ('asc-empty', 'ASC("")', '?ILLEGAL QUANTITY  ERROR'),
    ('mid-zero', 'MID$("A",0)', '?ILLEGAL QUANTITY  ERROR'),
]:
    expression('functions.' + name, value, expected)
program('arrays.numeric', ['10 DIM A(10)', '20 FOR I=0 TO 10',
    '30 A(I)=I*2', '40 NEXT I', '50 PRINT A(5)'], '10')
case('arrays.string-multidimensional', ('DIM A$(2,3):A$(2,3)="MiXeD":PRINT A$(2,3)', 'MiXeD'),
     ('PRINT LEN(A$(0,0))', '0'))
case('arrays.implicit', ('A(10)=7:PRINT A(0);A(10)', '0  7'),
     ('PRINT A(11)', '?BAD SUBSCRIPT  ERROR'))
case('arrays.errors', ('DIM A(2)', ''), ('DIM A(2)', '?REDIM\'D ARRAY  ERROR'),
     ('PRINT A(3)', '?BAD SUBSCRIPT  ERROR'),
     ('PRINT A(-1)', '?ILLEGAL QUANTITY  ERROR'))
case('arrays.maximum-empty-arena', ('DIM A(7779):A(7779)=1:PRINT A(7779)', '1'),
     ('CLR', ''), ('DIM A(7780)', '?OUT OF MEMORY  ERROR'), ('PRINT 2+2', '4'))
case('arrays.capacity', ('DIM A(7000):A(7000)=123:PRINT A(7000)', '123'),
     ('CLR', ''), ('DIM A(8000)', '?OUT OF MEMORY  ERROR'), ('PRINT 2+2', '4'))
program('if.statements', ['10 A=10', '20 IF A=10 THEN PRINT "PASS"',
    '30 IF A<>10 THEN PRINT "FAIL"'], 'PASS')
program('if.line-target', ['10 IF 1 THEN 30', '20 PRINT "FAIL"',
    '30 IF 0 THEN PRINT "FAIL":PRINT "FAIL"', '40 PRINT "PASS"'], 'PASS')
program('goto.forward', ['10 PRINT "A"', '20 GOTO 40', '30 PRINT "FAIL"',
    '40 PRINT "B"'], 'A\nB')
program('gosub.return', ['10 GOSUB 100', '20 PRINT "DONE":END',
    '100 PRINT "SUB":GOSUB 200', '110 RETURN', '200 PRINT "NESTED":RETURN'], 'SUB\nNESTED\nDONE')
case('gosub.orphan', ('RETURN', '?RETURN WITHOUT GOSUB  ERROR'))
program('for.positive', ['10 FOR I=1 TO 5', '20 PRINT I', '30 NEXT I'], '1 \n 2 \n 3 \n 4 \n 5')
program('for.negative', ['10 FOR I=3 TO 1 STEP -1:PRINT I:NEXT'], '3 \n 2 \n 1')
program('for.fractional', ['10 FOR I=0 TO 1 STEP .5:PRINT I:NEXT'], '0 \n .5 \n 1')
program('for.nested', ['10 FOR I=1 TO 2:FOR J=1 TO 2:PRINT I;J:NEXT J,I'], '1  1 \n 1  2 \n 2  1 \n 2  2')
program('for.initial-past-limit', ['10 FOR I=2 TO 1:PRINT I:NEXT'], '2')
case('for.orphan', ('NEXT', '?NEXT WITHOUT FOR  ERROR'))
program('data.restore', ['10 DATA 1,-2.5,"Hello, TabOS",MiXeD',
    '20 READ A,B,C$,D$:PRINT A;B:PRINT C$:PRINT D$',
    '30 RESTORE:READ A:PRINT A'], '1 -2.5 \nHello, TabOS\nMiXeD\n 1')
program('data.exhausted', ['10 DATA 1', '20 READ A,B'], '?OUT OF DATA  ERROR IN 20')
program('data.type', ['10 DATA WORD', '20 READ A'], '?SYNTAX  ERROR IN 10')
program('def-fn.numeric', ['10 DEF FNA(X)=X*X+1', '20 PRINT FNA(3)'], '10')
case('def-fn.direct', ('DEF FNA(X)=X', '?ILLEGAL DIRECT  ERROR'))
expression('def-fn.undefined', 'FNA(1)', '?UNDEF\'D FUNCTION  ERROR')
program('on.goto', ['10 ON 2 GOTO 100,200,300', '20 PRINT "FAIL":END',
    '100 PRINT "FAIL":END', '200 PRINT "PASS":END', '300 PRINT "FAIL"'], 'PASS')
program('on.gosub', ['10 ON 2 GOSUB 100,200', '20 PRINT "DONE":END',
    '100 PRINT "FAIL":RETURN', '200 PRINT "PASS":RETURN'], 'PASS\nDONE')
program('on.fallthrough', ['10 ON 0 GOTO 100', '20 ON 3 GOSUB 100,100',
    '30 PRINT "PASS":END', '100 PRINT "FAIL":END'], 'PASS')
case('stop.cont', ('10 A=1:STOP', None), ('20 PRINT A+1:END', None),
     ('RUN', 'BREAK IN 10'), ('CONT', '2'))
case('end.cont', ('10 END', None), ('20 PRINT "CONTINUED"', None),
     ('RUN', ''), ('CONT', 'CONTINUED'))
# This silent empty-program CONT also reproduces in the pristine pinned upstream.
case('cont.empty', ('CONT', ''))
case('cont.edited', ('10 STOP', None), ('RUN', 'BREAK IN 10'),
     ('20 END', None), ('CONT', '?CAN\'T CONTINUE  ERROR'))
case('program.management', ('30 PRINT "OLD"', None), ('10 PRINT "FIRST"', None),
     ('30 PRINT "LAST"', None), ('20 PRINT "DELETE"', None), ('20', None),
     ('LIST', '10 PRINT "FIRST"\n30 PRINT "LAST"'),
     ('LIST 30-30', '30 PRINT "LAST"'), ('RUN', 'FIRST\nLAST'),
     ('RUN 30', 'LAST'), ('NEW', ''), ('LIST', ''))
case('clr', ('10 PRINT "STORED"', None), ('A=9:A$="X":DIM B(2):B(2)=7', ''),
     ('CLR', ''), ('PRINT A;LEN(A$);B(2)', '0  0  0'), ('LIST', '10 PRINT "STORED"'))
case('print.format', ('PRINT "A";"B":PRINT "C"', 'AB\nC'),
     ('PRINT "A", "B"', 'A         B'), ('?', ''))
for name, value, expected in [
    ('abs', 'ABS(-2)', '2'), ('int', 'INT(-1.2)', '-2'),
    ('sgn', 'SGN(-4);SGN(0);SGN(4)', '-1  0  1'), ('sqr', 'SQR(9)', '3'),
    ('sin', 'SIN(0)', '0'), ('cos', 'COS(0)', '1'), ('tan', 'TAN(0)', '0'),
    ('atn', 'ATN(0)', '0'), ('log', 'LOG(1)', '0'), ('exp', 'EXP(0)', '1'),
    ('sqr-domain', 'SQR(-1)', '?ILLEGAL QUANTITY  ERROR'),
    ('log-domain', 'LOG(0)', '?ILLEGAL QUANTITY  ERROR'),
]:
    expression('math.' + name, value, expected)
case('math.rnd', ('A=RND(-1):B=RND(1):C=RND(-1):D=RND(1)', ''),
     ('PRINT (A=C);(B=D);(D>=0 AND D<1);(RND(0)>=0)', '-1 -1 -1 -1'))
case('storage.roundtrip', ('10 PRINT "MiXeD"', None), ('SAVE "COMPAT"', ''),
     ('NEW', ''), ('LOAD "COMPAT"', ''), ('LIST', '10 PRINT "MiXeD"'),
     ('RUN', 'MiXeD'))
case('c64.peek', ('PRINT PEEK(0)', '0'), ('PRINT 2+2', '4'))
case('c64.poke', ('POKE 0,0', ''), ('PRINT PEEK(0)', '0'))
for name, command in [('sys', 'SYS 1'), ('usr', 'PRINT USR(0)'), ('wait', 'WAIT 1,1'),
    ('verify', 'VERIFY "X"'), ('open', 'OPEN 255,8,255,"X"'),
    ('close', 'CLOSE 255'), ('cmd', 'CMD 255')]:
    case('unsupported.' + name,
         (command, '?UNSUPPORTED IN TABOS STAGE B\n\n?ILLEGAL QUANTITY  ERROR'),
         ('PRINT 2+2', '4'))



program('if.strings', ['10 IF "A"<"B" THEN PRINT "PASS":PRINT "COLON"',
    '20 IF "A"="a" THEN PRINT "FAIL"'], 'PASS\nCOLON')
case('goto.missing', ('GOTO 999', "?UNDEF'D STATEMENT  ERROR"),
     ('GOSUB 999', "?UNDEF'D STATEMENT  ERROR"), ('GOTO -1', "?UNDEF'D STATEMENT  ERROR"))
program('for.zero-step', ['10 N=0:FOR I=1 TO 3 STEP 0',
    '20 N=N+1:IF N=3 THEN PRINT I:END', '30 NEXT I'], '1')
program('for.mismatched', ['10 FOR I=1 TO 2:NEXT J'], '?NEXT WITHOUT FOR  ERROR IN 10')
program('data.sample-numeric', ['10 DATA 1,2,3', '20 READ A,B,C',
    '30 PRINT A,B,C'], '1         2         3')
program('data.sample-strings', ['10 DATA HELLO,"Hello TabOS",42',
    '20 READ A$,B$,C', '30 PRINT A$', '40 PRINT B$', '50 PRINT C'], 'HELLO\nHello TabOS\n 42')
program('data.after-statement', ['10 A=1:DATA MiXeD,"a,b"',
    '20 READ A$:READ B$:PRINT A$;":";B$'], 'MiXeD:a,b')
case('print.spacing', ('PRINT 1;2;3', '1  2  3'),
     ('PRINT "A";:PRINT "B"', 'AB'), ('PRINT "A";SPC(3);"B"', 'A   B'),
     ('PRINT "A";TAB(5);"B"', 'A    B'))
for name, value, expected in [
    ('asc', 'ASC("A")', '65'), ('chr', 'CHR$(65)', 'A'),
    ('val', 'VAL("123")', '123'), ('str', 'STR$(123)', '123'),
    ('left-zero', 'LEN(LEFT$("ABC",0))', '0'),
    ('right-long', 'RIGHT$("ABC",10)', 'ABC'),
    ('mid-beyond', 'LEN(MID$("ABC",9))', '0'),
    ('empty', 'LEN(LEFT$("",2))', '0'), ('val-invalid', 'VAL("XYZ")', '0'),
    ('chr-high', 'ASC(CHR$(255))', '255'),
    ('chr-invalid', 'CHR$(256)', '?ILLEGAL QUANTITY  ERROR'),
]:
    expression('functions.' + name, value, expected)
for name, value in [('equal', '1=1'), ('not-equal', '1<>2'), ('less', '1<2'),
    ('greater', '2>1'), ('less-equal', '1<=1'), ('greater-equal', '1>=1'),
    ('mixed', '(1+2*3=7) AND NOT (2<1)')]:
    expression('comparison.' + name, value, '-1')
program('def-fn.scope', ['10 X=7:DEF FNA(X)=X*X',
    '20 PRINT FNA(5);FNA(3)+1;X'], '25  10  7')
program('on.fractional', ['10 ON 1.9 GOTO 100,200',
    '100 PRINT "ONE":END', '200 PRINT "FAIL":END'], 'ONE')
case('on.invalid', ('ON -1 GOTO 10', '?ILLEGAL QUANTITY  ERROR'),
     ('ON 256 GOTO 10', '?ILLEGAL QUANTITY  ERROR'))
case('list.lexical', ('10 print "MiXeD"', None), ('20 rem Keep Case', None),
     ('30 data MiXeD,"a,b"', None),
     ('LIST 10', '10 PRINT "MiXeD"'),
     ('LIST 20-30', '20 REM Keep Case\n30 DATA MiXeD,"a,b"'))
case('run.clears', ('10 PRINT A;LEN(A$);B(2)', None),
     ('A=5:A$="X":B(2)=7', ''), ('RUN', '0  0  0'))
case('errors.types', ('PRINT 1+"A"', '?TYPE MISMATCH  ERROR'),
     ('LET 1=2', '?SYNTAX  ERROR'), ('PRINT 4', '4'))
program('get.empty', ['10 GET A$:PRINT LEN(A$)', '20 GET A:PRINT A'], '0 \n 0')
program('math.approximate', [
    '10 PRINT ABS(SIN(1)-.841470985)<.000001',
    '20 PRINT ABS(COS(1)-.540302306)<.000001',
    '30 PRINT ABS(TAN(1)-1.55740772)<.000001',
    '40 PRINT ABS(ATN(1)-.785398163)<.000001',
    '50 PRINT ABS(LOG(2)-.693147181)<.000001',
    '60 PRINT ABS(EXP(1)-2.71828183)<.000001'], '-1 \n-1 \n-1 \n-1 \n-1 \n-1')
program('physical.sample', ['10 DATA 1,2,3', '20 FOR I=1 TO 3', '30 READ A',
    '40 GOSUB 100', '50 NEXT I', '60 END', '100 PRINT "VALUE";A',
    '110 RETURN'], 'VALUE 1 \nVALUE 2 \nVALUE 3')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, nargs='?')
    parser.add_argument('--rv32-corpus', type=Path)
    parser.add_argument('--transcript', type=Path)
    args = parser.parse_args()
    if args.rv32_corpus:
        rows = []
        for name, steps in CASES:
            for command, expected in [('NEW', ''), *steps]:
                encoded = '@LINE@' if expected is None else expected.strip().replace('\n', r'\n')
                rows.append(name + '\t' + command + '\t' + encoded)
        args.rv32_corpus.write_text('\n'.join(rows) + '\n')
        return
    if args.executable is None:
        parser.error('executable is required')
    evidence = []
    failures = []
    with tempfile.TemporaryDirectory(prefix='tabos-basic-compat-') as root, tempfile.TemporaryFile() as errors:
        Path(root, 'T:').mkdir()
        process = subprocess.Popen([str(args.executable.resolve())], cwd=root,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors)
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        pending = b''

        def receive(marker):
            nonlocal pending
            deadline = time.monotonic() + 20
            while marker not in pending:
                if time.monotonic() >= deadline:
                    raise AssertionError(('timeout', marker, pending))
                for key, _ in selector.select(.1):
                    chunk = os.read(key.fd, 8192)
                    if not chunk:
                        raise AssertionError(('unexpected exit', process.poll(), pending))
                    pending += chunk
            end = pending.index(marker) + len(marker)
            result, pending = pending[:end], pending[end:]
            return result

        def run(command, expected):
            assert len(command) <= 80, command
            wire = command.encode('ascii') + b'\n'
            process.stdin.write(wire)
            process.stdin.flush()
            echo = receive(b'\n')
            assert echo == wire, (command, 'echo', echo)
            if expected is None:
                return ''
            result = receive(b'READY.\n')[:-len(b'READY.\n')].decode('ascii').strip()
            if result != expected.strip():
                failures.append((command, expected, result))
                print('MISMATCH', repr(failures[-1]), flush=True)
            return result

        try:
            receive(b'READY.\n')
            for name, steps in CASES:
                run('NEW', '')
                record = {'id': name, 'steps': []}
                for command, expected in steps:
                    result = run(command, expected)
                    record['steps'].append({'command': command, 'expected': expected, 'actual': result})
                evidence.append(record)
                print('CHECKED', name, flush=True)

            def dialogue(name, lines, exchanges):
                run('NEW', '')
                for line in lines:
                    run(line, None)
                record = {'id': name, 'steps': []}
                for wire, marker, expected in exchanges:
                    process.stdin.write(wire)
                    process.stdin.flush()
                    actual = receive(marker).decode('ascii')
                    if actual != expected:
                        failures.append((name, expected, actual))
                        print('MISMATCH', repr(failures[-1]), flush=True)
                    record['steps'].append({'input': repr(wire), 'expected': expected, 'actual': actual})
                evidence.append(record)
                print('CHECKED', name, flush=True)

            dialogue('input.prompt-redo', ['10 INPUT "NUMBER";A', '20 PRINT A'], [
                (b'RUN\n', b'? ', 'RUN\nNUMBER? '),
                (b'WORD\n', b'? ', 'WORD\n?REDO FROM START\nNUMBER? '),
                (b'12\n', b'READY.\n', '12\n 12 \n\nREADY.\n')])
            dialogue('input.too-few', ['10 INPUT A,B', '20 PRINT A;B'], [
                (b'RUN\n', b'? ', 'RUN\n? '),
                (b'1\n', b'?? ', '1\n?? '),
                (b'2\n', b'READY.\n', '2\n 1  2 \n\nREADY.\n')])
            dialogue('input.extra', ['10 INPUT A,B', '20 PRINT A;B'], [
                (b'RUN\n', b'? ', 'RUN\n? '),
                (b'1,2,3\n', b'READY.\n', '1,2,3\n?EXTRA IGNORED\n 1  2 \n\nREADY.\n')])
            dialogue('input.quoted', ['10 INPUT A$', '20 PRINT A$'], [
                (b'RUN\n', b'? ', 'RUN\n? '),
                (b'"Hello, TabOS"\n', b'READY.\n', '"Hello, TabOS"\nHello, TabOS\n\nREADY.\n')])
            dialogue('input.empty', ['10 A=7:INPUT A', '20 PRINT A'], [
                (b'RUN\n', b'? ', 'RUN\n? '),
                (b'\n', b'READY.\n', '\n 7 \n\nREADY.\n')])
            dialogue('get.sequence', ['10 PRINT "KEYS"', '20 FOR I=1 TO 4',
                '30 GET A$:IF A$="" THEN 30', '40 PRINT A$:NEXT'], [
                (b'RUN\n', b'KEYS\n', 'RUN\nKEYS\n'),
                (b'a9!?', b'READY.\n', 'a\n9\n!\n?\n\nREADY.\n')])
            process.stdin.write(b'\x11')
            process.stdin.flush()
            assert process.wait(timeout=10) == 0
            errors.seek(0)
            diagnostics = errors.read()
            assert not diagnostics, diagnostics
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdin.close()
            process.stdout.close()
            selector.close()
            if args.transcript:
                args.transcript.write_text(json.dumps(evidence, indent=2) + '\n')
    assert not failures, failures
    print(f'{len(evidence)} compatibility cases passed; clean exit and empty stderr')


if __name__ == '__main__':
    main()
