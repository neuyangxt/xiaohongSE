#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_FILE="${PROJECT_ROOT}/components/openharmony_liteos/CMakeLists.txt"

case "$MODE" in
    all)
        BRINGUP=1
        KERNEL=1
        SAMGR=1
        ;;
    kernel)
        BRINGUP=0
        KERNEL=1
        SAMGR=0
        ;;
    quiet)
        BRINGUP=0
        KERNEL=0
        SAMGR=0
        ;;
    *)
        echo "Usage: $0 {all|kernel|quiet}"
        echo ""
        echo "  all    : S9-S32 + S7/S8 continuous smoke, current full verification mode"
        echo "  kernel : only S7/S8 queue, sem/event, mux/memory continuous smoke"
        echo "  quiet  : disable bringup smoke and continuous smoke"
        exit 1
        ;;
esac

python3 - "$CMAKE_FILE" "$BRINGUP" "$KERNEL" "$SAMGR" <<'PY'
from pathlib import Path
import re
import sys

cmake = Path(sys.argv[1])
bringup, kernel, samgr = sys.argv[2], sys.argv[3], sys.argv[4]

s = cmake.read_text()

defs = {
    "OHOS_ENABLE_BRINGUP_SMOKE": bringup,
    "OHOS_ENABLE_KERNEL_CONTINUOUS_SMOKE": kernel,
    "OHOS_ENABLE_SAMGR_RUNTIME_VERIFY": samgr,
}

for key, value in defs.items():
    if re.search(rf"{key}=\d", s):
        s = re.sub(rf"{key}=\d", f"{key}={value}", s)
    else:
        marker = "SHARED_TASK_STACK_SIZE=0x800U"
        if marker not in s:
            raise SystemExit(f"cannot find marker: {marker}")
        s = s.replace(marker, marker + f"\n    {key}={value}", 1)

cmake.write_text(s)

print(f"OHOS smoke mode updated:")
for key, value in defs.items():
    print(f"  {key}={value}")
PY
