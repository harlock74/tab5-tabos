"""Bounded interactive driver shared by graphics tests and example generation."""
import os
from pathlib import Path
import selectors
import struct
import subprocess
import tempfile
import time

class Session:
    def __init__(self, executable, root, args=(), **extra):
        self.checks = 0
        self.root = Path(root)
        self.trace = self.root/'media.log'
        self.pixels = self.root/'pixels.bin'
        env = dict(os.environ, TABOS_BASIC_MEDIA_TRACE=str(self.trace),
                   TABOS_BASIC_MEDIA_PIXELS=str(self.pixels), TABOS_BASIC_TEST_KEYS='1', **extra)
        self.errors = tempfile.TemporaryFile()
        self.p = subprocess.Popen([executable,*args], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=self.errors, cwd=root, env=env)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.p.stdout, selectors.EVENT_READ)
        self.receive()
    def send(self, value):
        self.p.stdin.write(value)
        self.p.stdin.flush()
    def receive(self, marker=b'READY.\n', timeout=8):
        output = b''
        end = time.monotonic()+timeout
        while marker not in output:
            assert time.monotonic()<end, (marker, output[-3000:])
            for key,_ in self.selector.select(.05):
                chunk = os.read(key.fd,8192)
                assert chunk, (self.p.poll(),output)
                output += chunk
                assert len(output)<100000, output[:3000]
        return output
    def command(self, command, expected=''):
        self.send(command.encode()+b'\n')
        out = self.receive()
        echo, result = out.split(b'\n',1)
        assert echo.decode() == command, (command,out)
        result = result.removesuffix(b'READY.\n').strip().decode()
        assert result == expected, (command,result,expected)
        self.checks += 1
    def line(self, command):
        self.send(command.encode()+b'\n')
        assert self.receive(command.encode()+b'\n') == command.encode()+b'\n'
    def pixel(self,x,y):
        return struct.unpack_from('<H',self.pixels.read_bytes(),2*(y*320+x))[0]
    def close(self):
        try:
            if self.p.poll() is None:
                self.send(b'\x11')
                assert self.p.wait(timeout=3)==0
            self.errors.seek(0)
            assert not self.errors.read(), 'sanitizer/runtime diagnostics'
        finally:
            if self.p.poll() is None:
                self.p.kill();self.p.wait()
            self.p.stdin.close();self.p.stdout.close();self.errors.close();self.selector.close()
