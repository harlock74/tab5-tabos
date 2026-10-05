# BASIC upstream provenance

Origin: https://github.com/mist64/cbmbasic

Pinned commit: `3b9e48d370241deded3bc3341ef7c8fc319bbb1f`
(2026-04-03, “Fix infinite error loop on CTRL-D / EOF”).

Imported files: generated `cbmbasic.c` and `glue.h`. Their original SHA-256 values:

| File | Original SHA-256 |
| --- | --- |
| cbmbasic.c | `3b205cd62ab1e554fda030e511430c1b8a8d92077ecbc53a9fa7adafaff5a5e7` |
| glue.h | `6d1e5ccf63f9f51e73c45ecbeb3bc81a47ddcd58b6f178cefe7e4cf1ddcccb53` |

`src/basic_kernal.c` adapts the character-fetch algorithm and KERNAL conventions
from upstream `runtime.c`, original SHA-256
`60cd7b4d97d8d2ba346dc911faa9cd01c476ba23bcbf95f9ab7bb09461ef9306`.
The desktop runtime, console, plugins, filesystem implementation, platform
compatibility files and upstream build files are not imported or compiled.

Copyright (c) 2009 Michael Steil, James Abbatiello. The exact two-condition
BSD-style notice and disclaimer are retained in the core and in [LICENSE](LICENSE).
Distribution must retain these materials. Installation places LICENSE and this
provenance file under `T:/share/licenses/basic/`.

The core is static translation expressed in generated native C, with address-based
dispatch and private 64 KiB language RAM. It retains sparse ROM-derived constants
and initialization bytes, including tokens, numeric tables, messages and vectors.
There is no imported C64 ROM file or 6502 emulator. Stage A identified this
provenance qualification; the user accepted using the pinned translated core for
Stage B. This acceptance is not legal clearance of underlying Commodore/Microsoft-
derived material. No claim of a clean-room implementation is made.

## Reproducible local patch

`upstream/patches/tabos.patch` applies to the exact original two files:

```sh
cd /path/to/pristine/pinned/cbmbasic
patch -p1 < /path/to/tab5-tabos/apps/basic/upstream/patches/tabos.patch
```

Changes are limited to:

- Rename the generated main definition/declaration and glue declaration to
  `basic_core_main`, allowing an ordinary SDK main wrapper.
- Declare application-local hooks. WAIT (`$B82D`) and SYS (`$E12A`) remain rejected
  before address use or dispatch. Stage I retains BASIC's original numeric parsing
  for PEEK/POKE, then replaces only the final byte read/write at `$B816/$B827` with
  calls into the separate virtual C64 state. Failed virtual accesses enter BASIC's
  existing illegal-quantity error path (`$B248`).
- Reject USR's `$0310` trampoline at `llvm_cbe_not_found`, before the existing
  indirect JMP handling can follow its RAM vector.

Stage H additionally adds narrow hooks at the existing keyword scanner, token
listing, statement dispatch and expression dispatch, plus error/break cleanup.
An application-local continuation bridge calls only the existing expression and
string-descriptor helpers and resumes without recursive interpreter entry. Native
keywords use new CC–DD tokens; no old token or machine-address command is reused.

This gates executed statements/functions, including stored and conditional forms;
it is not a textual input blacklist. The generated initialization scratch remains
on the stack. Stage H's continuation dispatch and Stage I's two byte-operation
hooks are explicit in the reproducible patch. No desktop plugin or wholesale
formatting change is imported.

Check both exact original hashes by reversing the patch in temporary storage:

```sh
python3 apps/basic/tests/verify_upstream.py
```
