#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(script: Path, *args: str, expected: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run([sys.executable, str(script), *args], text=True, capture_output=True, check=False)
    if result.returncode != expected:
        raise AssertionError(
            f"command returned {result.returncode}, expected {expected}: {' '.join(args)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def compile_fixture(root: Path) -> None:
    (root / "lib").mkdir()
    (root / "dep.c").write_text("int package_audit_dep(void) { return 42; }\n", encoding="utf-8")
    (root / "main.c").write_text(
        "extern int package_audit_dep(void); int main(void) { return package_audit_dep() != 42; }\n",
        encoding="utf-8",
    )
    subprocess.run(
        ["cc", "-shared", "-fPIC", "-Wl,-soname,libpackage-audit-dep.so.1", "-o",
         str(root / "lib/libpackage-audit-dep.so.1"), str(root / "dep.c")],
        check=True,
    )
    os.symlink("libpackage-audit-dep.so.1", root / "lib/libpackage-audit-dep.so")
    subprocess.run(
        ["cc", "-o", str(root / "app"), str(root / "main.c"), f"-L{root / 'lib'}",
         "-lpackage-audit-dep", "-Wl,-rpath,$ORIGIN/lib"],
        check=True,
    )
    subprocess.run(["strip", "--strip-unneeded", str(root / "app"), str(root / "lib/libpackage-audit-dep.so.1")],
                   check=True)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: PackageAuditTest.py <package-audit.py>")
    script = Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "one").write_bytes(b"duplicate")
        (root / "nested").mkdir()
        (root / "nested/two").write_bytes(b"duplicate")
        inventory = run(script, "inventory", str(root))
        assert inventory.stdout.splitlines() == sorted(inventory.stdout.splitlines())
        assert "UNIQUE_REGULAR_BYTES\t9" in inventory.stderr
        assert "DUPLICATE\t" in inventory.stderr
        os.symlink("/outside", root / "escape")
        escaping = run(script, "inventory", str(root), expected=1)
        assert "ESCAPING_SYMLINK\tescape" in escaping.stderr

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        compile_fixture(root)
        run(script, "elf", str(root), "--root", "app", "--allow-system", "libc.so.6")
        orphan = root / "lib/liborphan.so.1"
        subprocess.run(
            ["cc", "-shared", "-fPIC", "-Wl,-soname,liborphan.so.1", "-o", str(orphan), str(root / "dep.c")],
            check=True,
        )
        subprocess.run(["strip", "--strip-unneeded", str(orphan)], check=True)
        unreachable = run(
            script, "elf", str(root), "--root", "app", "--allow-system", "libc.so.6", expected=1
        )
        assert "UNREACHABLE\tlib/liborphan.so.1" in unreachable.stdout
        orphan.unlink()
        (root / "lib/libpackage-audit-dep.so").unlink()
        run(script, "elf", str(root), "--root", "app", "--allow-system", "libc.so.6")
        (root / "lib/libpackage-audit-dep.so.1").unlink()
        missing = run(script, "elf", str(root), "--root", "app", "--allow-system", "libc.so.6", expected=1)
        assert "missing ELF dependency" in missing.stderr
    return 0

