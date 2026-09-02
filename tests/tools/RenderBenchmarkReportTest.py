#!/usr/bin/env python3
"""What the render-benchmark gate must and must not fail on.

The whole point of the gate is that it survives a shared CI runner, so the
cases that matter are the two failure modes it sits between: a scheduling
pause that must not fail the build, and a real regression that must.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def sample(route: str, gap: float, *, wall: float = 6.0, cpu: float = 2.0, construct: float = 0.0) -> dict:
    return {
        "routeFrom": "home",
        "routeTo": route,
        "maxGapMs": gap,
        "wallMs": wall,
        "guiCpuMs": cpu,
        "instanceMs": construct,
        "actualSwaps": 1,
        "frameBudgetMs": 16.666667,
    }


def report(samples: list[dict], *, idle: list[float] | None = None, cold: bool = False,
           backend: str | None = None) -> dict:
    document = {"iterations": 4, "cold": cold, "samples": samples}
    if idle is not None:
        document["idleGapsMs"] = idle
    if backend is not None:
        document["quickBackend"] = backend
    return document


def run(script: Path, current: dict, *, baseline: dict | None = None, expected: int = 0) -> str:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        current_path = root / "current.json"
        current_path.write_text(json.dumps(current), encoding="utf-8")
        args = [sys.executable, str(script), str(current_path)]
        if baseline is not None:
            baseline_path = root / "baseline.json"
            baseline_path.write_text(json.dumps(baseline), encoding="utf-8")
            args += ["--baseline", str(baseline_path)]
        result = subprocess.run(args, text=True, capture_output=True, check=False)
    if result.returncode != expected:
        raise AssertionError(
            f"gate returned {result.returncode}, expected {expected}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result.stdout


def main() -> int:
    script = Path(sys.argv[1])

    # A quiet run with everything inside the budget.
    quiet = [sample("home", 0.0) for _ in range(4)] + [sample("search", 0.0) for _ in range(4)]
    run(script, report(quiet, idle=[0.5] * 8))

    # One switch stalls badly and the rest are clean: a hosted runner being
    # descheduled, which is exactly what used to fail the build. The worst gap
    # is still reported, but it does not decide anything.
    outlier = [sample("home", 0.0), sample("home", 131.6), sample("home", 0.0), sample("home", 0.0)]
    output = run(script, report(outlier, idle=[0.5] * 4))
    assert "131.6 ms" in output, output
    assert "Regressions" not in output, output

    # A page that really did get slower: over the budget in most of its
    # samples, not one of them.
    regressed = [sample("home", 71.0), sample("home", 68.0), sample("home", 74.0), sample("home", 70.0)]
    output = run(script, report(regressed, idle=[0.5] * 4), expected=1)
    assert "median frame gap" in output, output

    # The same medians on a machine whose own idle drift is just as bad. The
    # app is not what is slow here, and the run has to pass.
    output = run(script, report(regressed, idle=[60.0] * 4))
    assert "Regressions" not in output, output

    # Cold runs are tracked, not gated: rebuilding a page cannot be free.
    run(script, report(regressed, idle=[0.5] * 4, cold=True))

    # The same sustained gap, painted by the software rasteriser. On a small
    # runner that is CPU rasterisation time, not a dropped frame, and the idle
    # probe cannot see it because the app is not idle while it paints. Reported,
    # not gated -- this is what used to fail the build for no good reason.
    output = run(script, report(regressed, idle=[0.5] * 4, backend="software"))
    assert "not gated" in output, output
    assert "Regressions" not in output, output

    # A real GPU backend still gates: there the gap means what it says.
    output = run(script, report(regressed, idle=[0.5] * 4, backend="rhi"), expected=1)
    assert "median frame gap" in output, output

    # A report from before the idle probe existed still gates on the budget,
    # rather than silently passing everything for want of a noise floor.
    output = run(script, report(regressed), expected=1)
    assert "No idle probe" in output, output

    # CPU time is compared against the baseline, because it does not advance
    # while the process is descheduled. It is compared as a share of the run,
    # so the walk has to have more than one route in it for the comparison to
    # be able to tell a slower page from a slower machine.
    def walk(home_cpu: float, cold: bool = False) -> dict:
        samples = []
        for _ in range(4):
            samples.append(sample("home", 0.0, cpu=home_cpu))
            samples.append(sample("search", 0.0, cpu=2.0))
            samples.append(sample("settings", 0.0, cpu=2.0))
        return report(samples, idle=[0.5] * 12, cold=cold)

    baseline = walk(2.0)
    run(script, walk(2.0), baseline=baseline)

    # One page doing twice the work it used to, while the rest of the walk is
    # unchanged. That is a regression and has to fail.
    output = run(script, walk(4.0), baseline=baseline, expected=1)
    assert "guiCpuMs" in output, output
    assert "home:" in output, output

    # The same walk on a machine that is uniformly half the speed. Every route
    # moves together, nothing changed shape, and the run has to pass.
    slow_machine = report(
        [sample(route, 0.0, cpu=4.0) for _ in range(4) for route in ("home", "search", "settings")],
        idle=[0.5] * 12,
    )
    output = run(script, slow_machine, baseline=baseline)
    assert "Regressions" not in output, output

    # Cold runs are not gated on that either: a rebuilt page costs more CPU
    # than a cached one, which is the whole point of measuring it separately.
    output = run(script, walk(4.0, cold=True), baseline=baseline)
    assert "Regressions" not in output, output

    # Construct time is microseconds on a warm page, so a percentage of it is
    # reached by rounding. A regression has to be worth a fraction of a frame
    # in absolute terms before it counts.
    def construct_walk(home_construct: float) -> dict:
        samples = []
        for _ in range(4):
            samples.append(sample("home", 0.0, construct=home_construct))
            samples.append(sample("search", 0.0, construct=0.006))
        return report(samples, idle=[0.5] * 8)

    output = run(script, construct_walk(0.020), baseline=construct_walk(0.006))
    assert "Regressions" not in output, output

    # Wall-clock time is not, because the baseline came off another machine.
    slower_wall = report([sample("home", 0.0, wall=60.0) for _ in range(4)], idle=[0.5] * 4)
    output = run(script, slower_wall, baseline=baseline)
    assert "Regressions" not in output, output

    # An empty run is a broken run, not a clean one.
    run(script, report([]), expected=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
