#!/usr/bin/env python3
# Written for ocarina-of-time-recomp (MIT, Copyright (c) 2026 Jeff Aigner).
"""Find the compute kernel whose tensor-core lowering breaks the picture, by screenshots of a fixed scene.

    python3 tools/bisect_kernels.py [--hashes H1,H2,...] [--cache DIR] [--seconds 30]

Each test is one tools/run.py run: OOT_TENSOR_ONLY set to a subset of kernels (by the code hash tools/kernel.py
prints), an empty shader cache (the keys do not show the switch), and a screenshot after --seconds. A screenshot whose
noise score (tools/shot_diff.py) is above --noise-limit is broken. The tool first checks that all kernels together
break the picture, then halves the set until one kernel is left; if both halves of a broken set are fine, the
breakage needs kernels from both and it says so. Every test reads the run's log for the lines that say which programs
took the tensor path: a kernel that was translated but stayed emulated means the switch did not apply, and the
bisection stops (on 2026-11-06 an allow-list matched nothing and ten runs tested the emulation).
Default kernels: every compute program with HMMA in the cache's stored inputs (tools/kernel.py list --tensor).
At 30 s the title shows its grass loop (the castle and horse cinematic comes later, when the title idles).
Screenshots: build/logs/bisect-<n>.png. Needs Pillow and numpy (run with the venv that has them).
"""
import argparse
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kernel  # noqa: E402
import run  # noqa: E402
from shot_diff import noise_score  # noqa: E402


def default_hashes(cache: Path) -> list[str]:
    seen = []
    for p in sorted((cache / "in").glob("*.in")):
        k = kernel.load(p)
        if k["stage"] == "cs" and k["ops"][0x23c] and k["hash"] not in seen:
            seen.append(k["hash"])
    return seen


class Bisector:
    def __init__(self, seconds: int, limit: float):
        self.seconds, self.limit, self.n = seconds, limit, 0

    def test(self, subset: list[str]) -> bool:
        """True when the picture is broken with these kernels on the tensor path."""
        self.n += 1
        tag = f"bisect-{self.n}"
        shot = f"build/logs/{tag}.png"
        r = run.run_game(self.seconds, shot, {"OOT_TENSOR_ONLY": ",".join(h[:16] for h in subset)}, fresh_cache=tag)
        if not r["ok"] or not r.get("shot"):
            sys.exit(f"bisect: {tag}: the run failed ({r['log']}, exit {r['exit']})")
        text = Path(r["log"]).read_text(errors="replace")
        on = set(re.findall(r"OOT_TENSOR_ONLY: program ([0-9a-f]{16}) .*: tensor cores", text))
        off = set(re.findall(r"OOT_TENSOR_ONLY: program ([0-9a-f]{16}) .*: emulated", text))
        wrong = [h for h in subset if h in off and h not in on]
        if wrong:
            sys.exit(f"bisect: {tag}: {', '.join(wrong)} translated but emulated: the switch did not apply ({r['log']})")
        absent = [h for h in subset if h not in on]
        score = noise_score(np.asarray(Image.open(r["shot"]).convert("RGB")))
        broken = score > self.limit
        print(f"bisect: {tag}: {len(subset)} kernels on the tensor path -> noise {score:.2f}, "
              f"{'BROKEN' if broken else 'fine'}" + (f" (not used in this scene: {', '.join(absent)})" if absent else "")
              + f"  [{shot}, {Path(r['log']).name}]", flush=True)
        return broken


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog="\n".join(__doc__.splitlines()[2:]))
    ap.add_argument("--hashes", help="comma-separated code hashes (tools/kernel.py list --tensor)")
    ap.add_argument("--cache", type=Path, default=kernel.CACHE)
    ap.add_argument("--seconds", type=int, default=30)
    ap.add_argument("--noise-limit", type=float, default=1.2)
    a = ap.parse_args()
    hashes = [h.strip() for h in a.hashes.split(",")] if a.hashes else default_hashes(a.cache)
    if not hashes:
        sys.exit("bisect: no kernels")
    print(f"bisect: {len(hashes)} kernels: {', '.join(hashes)}")
    b = Bisector(a.seconds, a.noise_limit)
    if not b.test(hashes):
        print("bisect: the picture is fine with every kernel on the tensor path: nothing to find in this scene")
        return 0
    cur = hashes
    while len(cur) > 1:
        half = len(cur) // 2
        lo, hi = cur[:half], cur[half:]
        if b.test(lo):
            cur = lo
        elif b.test(hi):
            cur = hi
        else:
            print(f"bisect: both halves are fine alone; together they break it: {', '.join(cur)}")
            return 1
    print(f"bisect: the kernel is {cur[0]} (tools/kernel.py list --tensor names its input ids)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
