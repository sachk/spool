#!/usr/bin/env python3
"""Turn render-benchmark runs into a report, and fail on a regression.

The benchmark writes one sample per route switch. What matters per switch is
not the average but whether frames are dropped -- a switch that averages well
and drops one frame still reads as a stutter -- so the report leads with the
frame gap and the gate is set on it.

Getting that gate to mean something on a shared CI runner is the hard part.
maxGapMs is the lateness of a frame-budget timer, so it measures the machine's
scheduler as much as the app: a hypervisor pause lands in it as cleanly as a
real regression does. Two things keep that from failing the build at random.

Every sample is paired with an idle probe -- the same timer, measured over the
settle window between steps, when the app has nothing to do. Whatever it shows
is the machine's noise floor at that moment, and a transition gap is only
evidence of anything when it is worse than that.

And nothing is judged on a single sample. A route fails on its median, so a
regression has to be present in most of its samples; one outlier cannot fail a
run, which is precisely what a scheduling pause produces.

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

# Metrics measured in wall-clock time on a machine we do not control. They are
# reported, because a person reading the table wants them, but a regression is
# never called on them against a baseline recorded on some other runner.
WALL_CLOCK = {"maxGapMs", "wallMs"}

# What a baseline comparison may fail on. Thread CPU time does not advance
# while the process is descheduled, which already makes it steadier than
# wall-clock, but it is not free of the machine either: under contention the
# same work costs more cycles, and a loaded runner inflates every route by a
# third. So these are compared as a share of the run's own scale rather than
# in milliseconds -- a slow machine moves the whole run and cancels out, while
# one page doing more work than it used to does not.
GATED_AGAINST_BASELINE = ("guiCpuMs", "instanceMs")

# How far above the measured noise floor a gap has to sit before it is read as
# the app's fault rather than the machine's. The floor is itself a measurement
# and so has spread; this keeps a marginally noisier stretch from reading as a
# regression.
NOISE_MARGIN = 1.5

# A baseline comparison also has to clear an absolute margin. Some of these
# metrics sit near zero on a warm page -- construct time is microseconds when
# nothing is rebuilt -- and a percentage of nearly nothing is reached by
# rounding alone.
MIN_ABSOLUTE_REGRESSION_MS = 0.5


def load(path: Path) -> dict:
    with path.open() as handle:
        return json.load(handle)


def quantile(values: list[float], fraction: float) -> float:
    """Nearest-rank quantile. Small n here, so no interpolation games."""
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round(fraction * (len(ordered) - 1))))
    return ordered[index]


def noise_floor(report: dict) -> float | None:
    """What a frame-budget timer drifted by while the app was doing nothing.

    Returns None for a report from before the idle probe existed, which the
    caller reads as "no floor measured" rather than "the machine was quiet".
    """
    gaps = [float(gap) for gap in report.get("idleGapsMs", [])]
    if not gaps:
        return None
    # The high end, not the worst: one stalled window says less about the run
    # than the level the machine kept drifting back to.
    return quantile(gaps, 0.9)


def run_scale(report: dict) -> float:
    """How expensive this run was overall, in gui-thread CPU milliseconds.

    The median across every sample, so one route getting slower barely moves
    it while a slower or busier machine moves all of it.
    """
    values = [float(s.get("guiCpuMs", 0)) for s in report.get("samples", [])]
    values = [value for value in values if value > 0]
    return statistics.median(values) if values else 0.0


def summarise(report: dict) -> dict:
    """Median per metric, per route, plus the spread the gate needs."""
    by_route: dict[str, list[dict]] = defaultdict(list)
    for sample in report.get("samples", []):
        by_route[sample.get("routeTo", "?")].append(sample)

    summary = {}
    for route, samples in by_route.items():
        entry: dict = {
            key: statistics.median(float(s.get(key, 0)) for s in samples)
            for key, _ in METRICS
        }
        gaps = [float(s.get("maxGapMs", 0)) for s in samples]
        entry["__gaps__"] = gaps
        entry["__worstGap__"] = max(gaps) if gaps else 0.0
        entry["__count__"] = len(samples)
        summary[route] = entry
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
        help="fail when the median frame gap exceeds this; 0 uses the run's own refresh budget",
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
    baseline_report = load(args.baseline) if args.baseline and args.baseline.exists() else None
    baseline = summarise(baseline_report) if baseline_report else None

    # The two runs happened on different machines under different loads. This
    # is the factor between them, and it divides out of the gated metrics.
    scale = run_scale(report)
    baseline_scale = run_scale(baseline_report) if baseline_report else 0.0
    comparable = scale > 0 and baseline_scale > 0

    budget = args.budget_ms or float(samples[0].get("frameBudgetMs", 16.7))
    cold = report.get("cold", False)
    floor = noise_floor(report)
    # A gap has to clear both the frame budget and whatever the machine was
    # doing anyway. With no floor measured, the budget stands alone.
    threshold = budget if floor is None else max(budget, floor * NOISE_MARGIN)

    lines: list[str] = []
    lines.append(f"### Render benchmark ({'cold' if cold else 'warm'})")
    lines.append("")
    lines.append(f"Frame budget {budget:.2f} ms · {len(samples)} switches · "
                 f"{report.get('iterations', '?')} iterations")
    if floor is None:
        lines.append("")
        lines.append("No idle probe in this run: frame gaps are judged against the budget alone.")
    else:
        lines.append("")
        lines.append(f"Machine noise floor {floor:.1f} ms (idle timer drift, 90th percentile) · "
                     f"gap threshold {threshold:.1f} ms")
    lines.append("")
    header = "| route | " + " | ".join(label for _, label in METRICS) + " | worst gap |"
    lines.append(header)
    lines.append("|" + "---|" * (len(METRICS) + 2))

    failures: list[str] = []
    for route in sorted(current):
        entry = current[route]
        cells = [route]
        for key, _ in METRICS:
            value = entry[key]
            was = baseline.get(route, {}).get(key) if baseline else None
            unit = "" if key == "actualSwaps" else " ms"
            precision = 0 if key == "actualSwaps" else 1
            cells.append(f"{value:.{precision}f}{unit}{format_delta(value, was)}")
            # Wall-clock metrics are shown with their delta and left at that:
            # the baseline was recorded on a different machine on a different
            # day, so a change in them is not evidence of anything.
            if not cold and key in GATED_AGAINST_BASELINE and was and was > 0 and comparable:
                # Both sides as a share of their own run, so only a change in
                # this route relative to the rest of the walk can fail.
                share = value / scale
                was_share = was / baseline_scale
                # The absolute margin is checked on the baseline's machine
                # scale, so "worth a fraction of a frame" means the same thing
                # on a fast runner as on a slow one.
                absolute = (share - was_share) * baseline_scale
                if share > was_share * (1 + args.tolerance) and absolute >= MIN_ABSOLUTE_REGRESSION_MS:
                    failures.append(
                        f"{route}: {key} is {share / was_share - 1:+.0%} against the baseline "
                        f"as a share of the run ({value:.2f} ms now, {was:.2f} ms then)"
                    )
        cells.append(f"{entry['__worstGap__']:.1f} ms")
        lines.append("| " + " | ".join(cells) + " |")

    # A dropped frame is the thing this is here to catch, so it fails the run
    # on its own -- but only in warm mode, where a page is meant to be ready.
    # Cold numbers are tracked, not gated: rebuilding a page cannot be free.
    #
    # The median, not the worst: a route has to be over the line in most of its
    # samples. A single late frame is what a shared runner hands out for free.
    if not cold:
        for route in sorted(current):
            median_gap = current[route]["maxGapMs"]
            if median_gap > threshold:
                detail = f"exceeds the {budget:.1f} ms frame budget"
                if floor is not None and threshold > budget:
                    detail += f" and the {floor:.1f} ms noise floor"
                failures.append(
                    f"{route}: median frame gap {median_gap:.1f} ms {detail} "
                    f"across {current[route]['__count__']} switches"
                )

    worst = max((entry["__worstGap__"] for entry in current.values()), default=0.0)
    lines.append("")
    lines.append(f"Worst single frame gap across every switch: **{worst:.1f} ms** "
                 f"(informational; the gate is on the per-route median).")

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
