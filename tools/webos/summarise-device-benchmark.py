#!/usr/bin/env python3
"""Print a device benchmark report as a phase breakdown.

The totals alone hide where a switch spends its time, which is the whole
question on a television: a switch that is slow because it builds a page is a
different problem from one that is slow because the frame takes a long time to
reach the panel.
"""

from __future__ import annotations

import json
import statistics
import sys
from pathlib import Path


def median(values: list[float]) -> float:
    return statistics.median(values) if values else 0.0


def summarise_routes(report: dict) -> None:
    routes: dict[str, list[dict]] = {}
    for sample in report.get("samples", []):
        routes.setdefault(sample["routeTo"], []).append(sample)
    if not routes:
        return

    print(f"{'route':<20}{'wall':>10}{'construct':>11}{'ready':>9}{'present':>9}"
          f"{'gui cpu':>9}{'swaps':>7}{'worst gap':>11}")
    for route in sorted(routes):
        rows = routes[route]
        wall = median([r["wallMs"] for r in rows])
        construct = median([r.get("instanceMs", 0.0) for r in rows])
        ready = median([r.get("contentReadyMs", 0.0) for r in rows])
        present = median([r.get("presentMs", 0.0) for r in rows])
        cpu = median([r.get("guiCpuMs", 0.0) for r in rows])
        swaps = median([float(r.get("actualSwaps", 0)) for r in rows])
        gap = median([r.get("maxGapMs", 0.0) for r in rows])
        print(f"{route:<20}{wall:>7.1f} ms{construct:>8.1f} ms{ready:>6.1f} ms"
              f"{present:>6.1f} ms{cpu:>6.1f} ms{swaps:>7.0f}{gap:>8.1f} ms")

    # present is the part after the page says it is ready: the frames it takes
    # to actually reach the screen. When that dominates, the app is waiting on
    # the compositor and the panel rather than on its own work.
    walls = [s["wallMs"] for s in report.get("samples", [])]
    presents = [s.get("presentMs", 0.0) for s in report.get("samples", [])]
    if walls:
        share = median(presents) / median(walls) * 100 if median(walls) else 0
        print(f"\npresent is {share:.0f}% of the median switch "
              f"({median(presents):.1f} ms of {median(walls):.1f} ms)")


def summarise_scroll(report: dict) -> None:
    samples = report.get("scrollSamples") or []
    if not samples:
        return
    print(f"\n{'screen':<10}{'settle':>10}{'worst gap':>12}{'decode':>11}"
          f"{'images':>9}{'Mpixels':>10}")
    for sample in samples:
        print(f"{sample.get('screen', 0):<10}{sample.get('settleMs', 0):>7.1f} ms"
              f"{sample.get('maxGapMs', 0):>9.1f} ms{sample.get('decodeMsTotal', 0):>8.0f} ms"
              f"{sample.get('decodedImagesTotal', 0):>9}"
              f"{sample.get('decodedPixelsTotal', 0) / 1e6:>10.1f}")
    settles = [s.get("settleMs", 0) for s in samples]
    print(f"\nsettle median {median(settles):.1f} ms  max {max(settles):.1f} ms")


def main() -> int:
    path = Path(sys.argv[1])
    report = json.loads(path.read_text(encoding="utf-8"))
    idle = report.get("idleGapsMs") or []
    print(f"{path.name}: backend={report.get('quickBackend')} "
          f"cold={report.get('cold')} iterations={report.get('iterations')}"
          + (f" noise floor {statistics.median(idle):.1f} ms" if idle else ""))
    print()
    summarise_routes(report)
    summarise_scroll(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
