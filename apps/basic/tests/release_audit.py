#!/usr/bin/env python3
"""Validate the release example, provenance and installed-notice manifest."""

from hashlib import sha256
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
basic = root / "apps/basic"
examples = basic / "examples"

makefile = (basic / "Makefile").read_text()
match = re.search(r"^TABOS_RUNTIME_ASSETS := (.+)$", makefile, re.MULTILINE)
assert match, "missing TABOS_RUNTIME_ASSETS"
assets = [Path(item).name for item in match.group(1).split()]
sources = sorted(path.name for path in examples.glob("*.bas"))
programs = sorted(path.name for path in examples.glob("*.prg"))
assert sorted(assets) == programs, (assets, programs)
assert [Path(name).stem for name in sources] == [Path(name).stem for name in programs]

catalogue = (root / "docs/basic-examples.md").read_text()
for name in programs:
    assert catalogue.count(f"| `{name}` |") == 1, f"catalogue entry for {name}"

third_party = (examples / "THIRD_PARTY.md").read_text()
unlicense = (examples / "UNLICENSE").read_text()
upstream = (basic / "UPSTREAM.md").read_text()
license_text = (basic / "LICENSE").read_text()
assert "5301155192d91d74d337899cecc59dbda59c4c17" in third_party
assert "every other example" in third_party
assert "free and unencumbered software" in unlicense
assert "3b9e48d370241deded3bc3341ef7c8fc319bbb1f" in upstream
assert "3b205cd62ab1e554fda030e511430c1b8a8d92077ecbc53a9fa7adafaff5a5e7" in upstream
assert "6d1e5ccf63f9f51e73c45ecbeb3bc81a47ddcd58b6f178cefe7e4cf1ddcccb53" in upstream
assert "not legal clearance" in upstream and "clean-room" in upstream
assert "Copyright (c) 2009 Michael Steil, James Abbatiello" in license_text
assert "LICENSE UPSTREAM.md examples/THIRD_PARTY.md examples/UNLICENSE" in makefile

amazing_hash = sha256((examples / "AMAZING.bas").read_bytes()).hexdigest()
assert amazing_hash == "3f6fa3f264d148267a8216a4d9b57bdbaf2cf0c7fb346a4b60a8fe00728cd9f7"

if len(sys.argv) == 2:
    install = Path(sys.argv[1])
    assert (install / "bin/basic").is_file()
    for name in programs:
        assert (install / "data/basic" / name).read_bytes() == (examples / name).read_bytes(), name
    for name, source in [
        ("LICENSE", basic / "LICENSE"),
        ("UPSTREAM.md", basic / "UPSTREAM.md"),
        ("THIRD_PARTY.md", examples / "THIRD_PARTY.md"),
        ("UNLICENSE", examples / "UNLICENSE"),
    ]:
        assert (install / "share/licenses/basic" / name).read_bytes() == source.read_bytes(), name
elif len(sys.argv) != 1:
    raise SystemExit(f"usage: {sys.argv[0]} [installed-T-root]")

print(f"TabBASIC release manifest: {len(programs)} examples and required notices passed")
