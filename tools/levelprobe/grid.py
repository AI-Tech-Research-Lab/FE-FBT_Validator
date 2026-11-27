#!/usr/bin/env python3
"""Runs levelprobe over a grid of configurations and prints one summary line per configuration:
the last callable level and minimum effective input levels for noise scale degree 1 and 2, and the effective
levels left in the output. Usage: grid.py <grid-name> [jobs]"""
import itertools, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "build", "levelprobe")

def run(cfg):
    mode, st, skd, D, logN, logS, cts, stc, fdeg, sms, fms = cfg
    args = [BIN, mode, st, skd, str(D), str(logN), str(logS), str(cts), str(stc), str(fdeg), str(sms), str(fms)]
    env = dict(os.environ, OMP_NUM_THREADS="2")
    out = subprocess.run(args, capture_output=True, text=True, env=env).stdout
    res = {"cfg": cfg, "info": "", "s1": None, "s2": None}
    for line in out.splitlines():
        if line.startswith("INFO") or line.startswith("ERROR"):
            res["info"] = line
        if line.startswith("ROW"):
            t = line.split()
            sdeg, lvl, effIn, status = int(t[2]), int(t[4]), int(t[6]), t[7]
            outLevel, outDeg, effOut = int(t[9]), int(t[11]), int(t[13])
            key = "s%d" % sdeg
            if status == "ok" and res[key] is None:
                res[key] = {"last": lvl, "minEff": effIn, "effOut": effOut, "outLevel": outLevel, "outDeg": outDeg}
            if status == "wrong":
                res.setdefault("wrong", []).append((sdeg, lvl))
    return res

GRIDS = {
    # mode, st, skd, D, logN, logSlots, cts, stc, fdeg, sms, fms
    "smoke": [("bs1", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 24, 10, 5, 2, 2, 0, 50, 51)],
}

# Small rings keep every run around 100 MB and under a few seconds: N = 2^10, with 2^5 (sparse) or 2^9 (full)
# slots. Budget {1,1} (dense linear transforms) is only used with 2^5 slots.
LOGN = 10
STS = ["FLEXIBLEAUTO", "FLEXIBLEAUTOEXT", "FIXEDAUTO", "FIXEDMANUAL"]
SKDS = ["SPARSE_ENCAPSULATED", "SPARSE_TERNARY", "UNIFORM_TERNARY"]
BUDGETS = [(1, 1, 5), (3, 2, 9)]  # cts, stc, logSlots

def grid_main():
    g = []
    # FE-FBT (q0 = Delta + 1): scaling techniques x secret distributions x budgets
    for st in STS:
        for skd in SKDS:
            for cts, stc, logS in BUDGETS:
                g.append(("fefbt", st, skd, 30, LOGN, logS, cts, stc, 27, 50, 51))
    # degree transitions of GetDepthByDegree, and the degrees offered by the page
    for fdeg in [1, 2, 3, 4, 5, 6, 8, 13, 14, 16, 27, 28, 32, 59, 60, 64, 119, 120, 128, 247, 248, 256, 495, 496, 512,
                 1007, 1008, 1024]:
        g.append(("fefbt", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 5, 2, 2, fdeg, 50, 51))
    # SPARSE_ENCAPSULATED with q0 > 60 bits (Hamming weight 64, K = 28 table), and q0 = 60
    g.append(("fefbt", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 5, 2, 2, 27, 59, 60))
    g.append(("fefbt", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 5, 2, 2, 27, 60, 61))
    # sparse vs full slots, larger budgets
    g.append(("fefbt", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 9, 2, 2, 27, 50, 51))
    g.append(("fefbt", "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 9, 4, 4, 27, 50, 51))
    g.append(("fefbt", "FIXEDMANUAL", "SPARSE_ENCAPSULATED", 30, LOGN, 9, 4, 4, 27, 50, 51))
    # normal bootstrapping, 1 and 2 iterations
    for mode in ["bs1", "bs2"]:
        for st in STS:
            for skd in SKDS:
                for cts, stc, logS in BUDGETS:
                    g.append((mode, st, skd, 30, LOGN, logS, cts, stc, 0, 50, 60))
        g.append((mode, "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 9, 2, 2, 0, 50, 60))
        g.append((mode, "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 5, 2, 2, 0, 50, 51))
        g.append((mode, "FLEXIBLEAUTO", "SPARSE_ENCAPSULATED", 30, LOGN, 5, 2, 2, 0, 59, 61))
    return g

GRIDS["main"] = grid_main()

if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "smoke"
    jobs = int(sys.argv[2]) if len(sys.argv) > 2 else 1  # one probe at a time (memory)
    results = []
    with ThreadPoolExecutor(jobs) as ex:
        for r in ex.map(run, GRIDS[name]):
            results.append(r)
            c = r["cfg"]
            s1, s2 = r["s1"] or {}, r["s2"] or {}
            print("%-5s %-15s %-19s D=%d logS=%d b=%d,%d deg=%-3d sms=%d fms=%d | s1 last=%s min=%s out=%s (lvl %s deg %s) | s2 last=%s min=%s out=%s | %s %s" % (
                c[0], c[1], c[2], c[3], c[5], c[6], c[7], c[8], c[9], c[10],
                s1.get("last"), s1.get("minEff"), s1.get("effOut"), s1.get("outLevel"), s1.get("outDeg"),
                s2.get("last"), s2.get("minEff"), s2.get("effOut"), r["info"], r.get("wrong", "")), flush=True)
    with open(os.path.join(HERE, "build", "grid-%s.json" % name), "w") as f:
        json.dump(results, f, indent=1)
