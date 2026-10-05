#!/usr/bin/env python3
"""stub_sweep_to_toml.py - turn stub-sweep hits into [functions] entries for Battlefield: Bad Company.

Reads <exe_dir>/logs/stub_sweep.txt written by debug_tools::PerformStubSweep
(dev_debug_runtime = true) and classifies every hit address against the
functions codegen already knows (generated/default/badcompanyrecomp_recomp.*.cpp):

  NEW FUNCTION   address lies in a gap between known functions  -> `0xADDR = {}`
  ALREADY KNOWN  address is now a function start (stale log)     -> skipped
  MID-FUNCTION   address lies inside a known function's body     -> switch table / size fix

Usage:  python tools/stub_sweep_to_toml.py [path/to/stub_sweep.txt] [--toml]
"""

from __future__ import annotations

import glob
import re
import sys
from bisect import bisect_right
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "generated" / "default"
BUILD_DIRS = sorted(glob.glob(str(ROOT / "out" / "build" / "*")), key=lambda p: Path(p).stat().st_mtime, reverse=True)

FN_RE = re.compile(r"^DEFINE_REX_FUNC\((sub_[0-9A-F]{8})\) \{")
INS_RE = re.compile(r"^\s*// [a-z][a-z0-9.]*")
HIT_RE = re.compile(r"\[stub\] addr=0x([0-9A-Fa-f]{8}) LR=0x([0-9A-Fa-f]{8}) r3=0x([0-9A-Fa-f]{8})")


def function_spans() -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    for f in glob.glob(str(GEN / "badcompanyrecomp_recomp.*.cpp")):
        cur: int | None = None
        n = 0
        with open(f, encoding="utf-8", errors="replace") as fp:
            for line in fp:
                m = FN_RE.match(line)
                if m:
                    if cur is not None:
                        spans.append((cur, cur + 4 * n))
                    cur = int(m.group(1)[4:], 16)
                    n = 0
                    continue
                if cur is None:
                    continue
                if line.startswith("}"):
                    spans.append((cur, cur + 4 * n))
                    cur = None
                    continue
                if INS_RE.match(line):
                    n += 1
        if cur is not None:
            spans.append((cur, cur + 4 * n))
    spans.sort()
    return spans


def main(argv: list[str]) -> int:
    as_toml = "--toml" in argv
    args = [a for a in argv if not a.startswith("--")]
    if args:
        log = Path(args[0])
    else:
        cands = [Path(d) / "logs" / "stub_sweep.txt" for d in BUILD_DIRS]
        cands += [ROOT / "logs" / "stub_sweep.txt"]
        log = next((c for c in cands if c.exists()), None)
        if log is None:
            print("stub_sweep.txt not found under out/build/*/logs - run with dev_debug_runtime = true", file=sys.stderr)
            return 1

    hits: dict[int, tuple[int, int]] = {}
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        m = HIT_RE.search(line)
        if m:
            a = int(m.group(1), 16)
            hits.setdefault(a, (int(m.group(2), 16), int(m.group(3), 16)))
    if not hits:
        print(f"{log}: no [stub] hits")
        return 0

    spans = function_spans()
    starts = [s for s, _ in spans]
    new: list[int] = []
    known: list[int] = []
    mid: list[tuple[int, int]] = []
    for a in sorted(hits):
        i = bisect_right(starts, a) - 1
        if i >= 0 and starts[i] == a:
            known.append(a)
        elif i >= 0 and a < spans[i][1]:
            mid.append((a, starts[i]))
        else:
            new.append(a)

    if as_toml:
        for a in new:
            lr, r3 = hits[a]
            print(f"0x{a:08X} = {{}}   # stub sweep: LR=0x{lr:08X}")
        return 0

    print(f"log: {log}")
    print(f"hits: {len(hits)}  new functions: {len(new)}  already known: {len(known)}  mid-function: {len(mid)}")
    for a in new:
        lr, r3 = hits[a]
        i = bisect_right(starts, a) - 1
        prev_end = spans[i][1] if i >= 0 else 0
        nxt = starts[i + 1] if i + 1 < len(starts) else 0
        print(f"  NEW  0x{a:08X}  LR=0x{lr:08X}  gap 0x{prev_end:08X}..0x{nxt:08X}")
    for a, s in mid:
        lr, _ = hits[a]
        print(f"  MID  0x{a:08X}  inside sub_{s:08X}  LR=0x{lr:08X}  -> switch table / boundary fix")
    for a in known:
        print(f"  OLD  0x{a:08X}  already a function (stale log)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
