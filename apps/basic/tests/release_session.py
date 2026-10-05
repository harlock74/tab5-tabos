#!/usr/bin/env python3
"""Exercise a realistic mixed-mode release-candidate session."""

from pathlib import Path
import shutil
import sys
import tempfile
import time

from session import Session

executable = str(Path(sys.argv[1]).resolve())
examples = Path(__file__).resolve().parents[1] / "examples"

with tempfile.TemporaryDirectory(prefix="basic-release-session-") as root_name:
    root = Path(root_name)
    packaged = root / "T:/data/basic"
    packaged.mkdir(parents=True)
    for name in ["AMAZING.prg", "PONG.prg", "C64SCREEN.prg", "SIDTONE.prg", "C64DODGE.prg"]:
        shutil.copyfile(examples / name, packaged / name)

    session = Session(executable, root)
    try:
        session.command("PRINT 2+2", "4")
        session.line("10 INPUT A$:PRINT A$")
        session.send(b"RUN\n")
        session.receive(b"? ")
        session.send(b"Release session\n")
        session.receive(b"Release session\n")
        session.command('SAVE "SESSION"')
        session.command("NEW")
        session.command('LOAD "SESSION"')
        session.command("LIST", "10 INPUT A$:PRINT A$")

        for _ in range(3):
            session.command("GRAPHICS:CLS 0:PRESENT:TEXT")
        for frequency in [220, 440, 880]:
            session.command(f"SOUND {frequency},10")

        session.command('LOAD "C64SCREEN.prg"')
        session.command("RUN")
        session.command("TEXT")
        session.command('LOAD "SIDTONE.prg"')
        session.command("RUN")

        session.command('LOAD "PONG.prg"')
        session.send(b"RUN\n")
        session.receive(b"RUN\n")
        time.sleep(.08)
        session.send(b"\x03")
        assert b"BREAK IN" in session.receive()

        session.command('LOAD "C64DODGE.prg"')
        # Keep this temporary test copy running until Ctrl+C even in optimized
        # builds; the packaged source remains unchanged.
        session.line("170 OY=OY")
        session.send(b"RUN\n")
        session.receive(b"PRESS ENTER TO START? ")
        session.send(b"\n\x80")
        time.sleep(.06)
        session.send(b"\x81\x82")
        time.sleep(.06)
        session.send(b"\x83\x03")
        assert b"BREAK IN" in session.receive()
        session.command("PRINT 123", "123")
    finally:
        checks = session.checks
        session.close()

    trace = session.trace.read_text().splitlines()
    assert trace.count("CLOSE 0") >= 5, trace[-30:]
    assert trace.count("AUDIO_OPEN 0") >= 4, trace[-30:]
    assert trace[-1] in {"CLOSE 0", "AUDIO_CLOSE 0"}, trace[-10:]

    relaunched = Session(executable, root)
    try:
        relaunched.command("PRINT 456", "456")
    finally:
        checks += relaunched.checks
        relaunched.close()

print(f"TabBASIC extended text/storage/graphics/audio/C64/break/exit/relaunch session passed ({checks} commands)")
