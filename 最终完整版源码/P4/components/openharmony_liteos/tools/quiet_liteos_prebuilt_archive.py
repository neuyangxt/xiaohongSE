#!/usr/bin/env python3
from pathlib import Path
from datetime import datetime
import sys

archive = Path("components/openharmony_liteos/lib/libopenharmony_liteos_m.a")

patterns = [
    b"[TOP] pri=%u id=%u status=0x%x bitmap=0x%x q10_empty=%d q31_empty=%d\n",
    b"[TOP] idle-fallback id=%u status=0x%x bitmap=0x%x q10_empty=%d q31_empty=%d\n",
    b"[SWITCH-ENTER] runId=%u status=0x%x bitmap=0x%x q10_empty=%d q31_empty=%d idle=%u\n",
]

data = archive.read_bytes()
hits = sum(data.count(p) for p in patterns)

if hits == 0:
    print(f"{archive}: already quiet")
    sys.exit(0)

backup = archive.with_name(archive.name + ".bak_quiet_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
backup.write_bytes(data)

for pat in patterns:
    count = data.count(pat)
    print(f"{pat[:24]!r} count={count}")
    data = data.replace(pat, b"\0" * len(pat))

archive.write_bytes(data)
print(f"backup: {backup}")
print(f"{archive}: noisy strings nulled; run riscv32-esp-elf-ranlib afterwards")
