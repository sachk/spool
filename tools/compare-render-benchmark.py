#!/usr/bin/env python3
"""Turn render-benchmark runs into a report, and fail on a regression.

The benchmark writes one sample per route switch. What matters per switch is
not the average but the worst frame gap -- a switch that averages well and
drops one frame still reads as a stutter -- so the report leads with that and
the gate is set on it.

Usage:
  compare-render-benchmark.py current.json [--baseline baseline.json]
                              [--budget-ms 16.7] [--tolerance 0.25]
                              [--markdown report.md]
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

# What each number means, and which way is better. Only the ones a person
# would act on are reported; the raw samples keep everything.
METRICS = [
    ("maxGapMs", "worst frame gap"),
    ("wallMs", "wall"),
    ("guiCpuMs", "gui cpu"),
    ("instanceMs", "construct"),
    ("actualSwaps", "swaps"),
]


def load(path: Path) -> dict:
    with path.open() as handle:
        return json.load(handle)


def summarise(report: dict) -> dict:
    """Median per metric, per route, plus an overall worst gap."""
    by_route: dict[str, list[dict]] = defaultdict(list)
    for sample in report.get("samples", []):
        by_route[sample.get("routeTo", "?")].append(sample)

    summary = {}
    for route, samples in by_route.items():
        summary[route] = {
            key: statistics.median(float(s.get(key, 0)) for s in samples)
            for key, _ in METRICS
        }
    gaps = [float(s.get("maxGapMs", 0)) for s in report.get("samples", [])]
    summary["__worst__"] = {"maxGapMs": max(gaps) if gaps else 0.0}
    return summary


def format_delta(current: float, baseline: float | None) -> str:
    if baseline is None or baseline <= 0:
        return ""
    change = (current - baseline) / baseline
    if abs(change) < 0.02:
        return " (=)"
    return f" ({change:+.0%})"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("current", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument(
        "--budget-ms",
        type=float,
        default=0.0,
        help="fail when the worst frame gap exceeds this; 0 uses the run's own refresh budget",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.25,
        help="fractional regression against the baseline that fails the run",
    )
    parser.add_argument("--markdown", type=Path)
    args = parser.parse_args()

    report = load(args.current)
    samples = report.get("samples", [])
    if not samples:
        print("render benchmark: no samples recorded", file=sys.stderr)
        return 1

    current = summarise(report)
    baseline = summarise(load(args.baseline)) if args.baseline and args.baseline.exists() else None

    budget = args.budget_ms or float(samples[0].get("frameBudgetMs", 16.7))
    cold = report.get("cold", False)

    lines: list[str] = []
    lines.append(f"### Render benchmark ({'cold' if cold else 'warm'})")
    lines.append("")
    lines.append(f"Frame budget {budget:.2f} ms · {len(samples)} switches · "
                 f"{report.get('iterations', '?')} iterations")
    lines.append("")
    header = "| route | " + " | ".join(label for _, label in METRICS) + " |"
    lines.append(header)
    lines.append("|" + "---|" * (len(METRICS) + 1))

    failures: list[str] = []
    for route in sorted(key for key in current if not key.startswith("__")):
        cells = [route]
        for key, _ in METRICS:
            value = current[route][key]
            was = baseline.get(route, {}).get(key) if baseline else None
            unit = "" if key == "actualSwaps" else " ms"
            precision = 0 if key == "actualSwaps" else 1
            cells.append(f"{value:.{precision}f}{unit}{format_delta(value, was)}")
            if was and was > 0 and value > was * (1 + args.tolerance) and key in ("maxGapMs", "wallMs"):
                failures.append(
                    f"{route}: {key} {value:.1f} ms is more than "
                    f"{args.tolerance:.0%} worse than {was:.1f} ms"
                )
        lines.append("| " + " | ".join(cells) + " |")

    worst = current["__worst__"]["maxGapMs"]
    lines.append("")
    verdict = "within one frame" if worst <= budget else "drops frames"
    lines.append(f"Worst frame gap across every switch: **{worst:.1f} ms** ({verdict}).")

    # A dropped frame is the thing this is here to catch, so it fails the run
    # on its own -- but only in warm mode, where a page is meant to be ready.
    # Cold numbers are tracked, not gated: rebuilding a page cannot be free.
    if not cold and worst > budget:
        failures.append(f"worst frame gap {worst:.1f} ms exceeds the {budget:.1f} ms frame budget")

    if failures:
        lines.append("")
        lines.append("**Regressions**")
        for failure in failures:
            lines.append(f"- {failure}")

    text = "\n".join(lines)
    print(text)
    if args.markdown:
        args.markdown.write_text(text + "\n")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
