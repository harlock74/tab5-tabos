#!/usr/bin/env python3
"""Verify the imported source against pinned hashes without network access."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

upstream = Path(__file__).resolve().parents[1] / "upstream"
expected = {
    "cbmbasic.c": "3b205cd62ab1e554fda030e511430c1b8a8d92077ecbc53a9fa7adafaff5a5e7",
    "glue.h": "6d1e5ccf63f9f51e73c45ecbeb3bc81a47ddcd58b6f178cefe7e4cf1ddcccb53",
}
with tempfile.TemporaryDirectory(prefix="tabos-basic-provenance-") as directory:
    for name in expected:
        shutil.copyfile(upstream / name, Path(directory) / name)
    subprocess.run(["patch", "--batch", "-R", "-p1", "-i", str(upstream / "patches/tabos.patch")],
                   cwd=directory, check=True)
    for name, digest in expected.items():
        actual = hashlib.sha256((Path(directory) / name).read_bytes()).hexdigest()
        assert actual == digest, (name, actual)
        print(name, "matches pinned upstream SHA-256")
