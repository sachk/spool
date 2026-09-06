#!/usr/bin/env python3
"""Assemble one Gradle project that packages several ABIs into a single APK.

The per-ABI legs each produce a complete Android project and an APK built from
it. Everything in that project is the same between them except one file:
androiddeployqt writes res/values/libs.xml naming every bundled library with
its ABI in front of it, and Qt's loader reads those arrays at startup to decide
which architecture it is running. So the arrays cannot simply be inherited from
whichever leg ran first -- the loader would find no entry for the device it is
on and load nothing.

The union of those arrays is what a Qt multi-ABI package contains, and building
it is the only way to get one: libs.xml is compiled into resources.arsc, which
is not something to edit after the fact. This puts the union back into the
project and lets Gradle package it, so the universal APK is a build rather than
a splice of two.

The native libraries come from the per-ABI APKs rather than from their build
trees: those are the binaries Gradle already stripped and shipped, so the
universal APK carries exactly what the per-ABI downloads do.

Everything outside libs.xml is checked to be identical rather than assumed. A
version code that moved between two legs, or a manifest that did, would
otherwise be resolved silently in favour of whichever was read first.
"""

import argparse
import filecmp
import pathlib
import re
import shutil
import sys
import zipfile

LIBS_XML = pathlib.PurePosixPath("res/values/libs.xml")
GRADLE_PROPERTIES = pathlib.PurePosixPath("gradle.properties")
ABI_LIB_ENTRY = re.compile(r"^lib/([^/]+)/")
ARRAY_OPEN = re.compile(r"^\s*<array name=\"([^\"]+)\">\s*$")
ARRAY_CLOSE = re.compile(r"^\s*</array>\s*$")
TARGET_ABI_LIST = re.compile(r"^qtTargetAbiList=.*$", re.MULTILINE)


def apk_abi(path):
    """The single ABI a per-ABI APK carries native libraries for."""
    with zipfile.ZipFile(path) as archive:
        abis = set()
        for name in archive.namelist():
            match = ABI_LIB_ENTRY.match(name)
            if match:
                abis.add(match.group(1))
    if len(abis) != 1:
        sys.exit(f"error: {path} carries {len(abis)} ABIs ({', '.join(sorted(abis))}), expected one")
    return abis.pop()


def relative_files(root):
    return {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}


def comparable(root, name):
    """What of a file has to match, for the files that cannot match byte for byte.

    gradle.properties names the ABI it filters for, and that line is rewritten
    here anyway. The Qt jars are the same Java in every ABI's installation, but
    each was archived by its own build, so they are compared by what they hold
    rather than by the timestamps around it. Everything else has to be identical
    and is compared byte for byte.
    """
    path = root / name
    if name == GRADLE_PROPERTIES.as_posix():
        return TARGET_ABI_LIST.sub("qtTargetAbiList=", path.read_text(encoding="utf-8")).encode()
    if name.endswith(".jar"):
        with zipfile.ZipFile(path) as archive:
            return sorted((entry.filename, entry.CRC) for entry in archive.infolist())
    return None


def compare_projects(base, other, base_abi, other_abi):
    base_files = relative_files(base) - {LIBS_XML.as_posix()}
    other_files = relative_files(other) - {LIBS_XML.as_posix()}
    for name in sorted(other_files - base_files):
        sys.exit(f"error: the {other_abi} project carries {name}, which the {base_abi} one does not")
    for name in sorted(base_files - other_files):
        sys.exit(f"error: the {base_abi} project carries {name}, which the {other_abi} one does not")
    for name in sorted(base_files):
        normalised = comparable(base, name)
        if normalised is None:
            if filecmp.cmp(base / name, other / name, shallow=False):
                continue
        elif normalised == comparable(other, name):
            continue
        sys.exit(
            f"error: {name} differs between the {base_abi} and {other_abi} projects; "
            "they are not the same build"
        )


class ArrayItems:
    """Where one array's items sit in the file, so the rest survives verbatim."""

    def __init__(self, name):
        self.name = name

    def __eq__(self, other):
        return isinstance(other, ArrayItems) and other.name == self.name


def read_libs_arrays(path):
    """The <array> contents of a libs.xml, and the file with them emptied out.

    The template is what every ABI has to agree on; the arrays are what gets
    concatenated. Keeping the file as text preserves the comment and the shape
    the deployment tool wrote, which a round trip through an XML writer loses.
    """
    arrays = {}
    template = []
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        opened = ARRAY_OPEN.match(line)
        if opened:
            current = opened.group(1)
            arrays[current] = []
            template.append(line)
            continue
        if current is not None:
            if ARRAY_CLOSE.match(line):
                template.append(ArrayItems(current))
                template.append(line)
                current = None
                continue
            arrays[current].append(line)
            continue
        template.append(line)
    if current is not None:
        sys.exit(f"error: {path} leaves the {current} array unclosed")
    return arrays, template


def union_libs_xml(projects, abis, destination):
    base_arrays, template = read_libs_arrays(projects[abis[0]] / LIBS_XML)
    merged = {name: list(items) for name, items in base_arrays.items()}
    for abi in abis[1:]:
        arrays, other_template = read_libs_arrays(projects[abi] / LIBS_XML)
        if other_template != template:
            sys.exit(f"error: the {abi} libs.xml is not shaped like the {abis[0]} one")
        if set(arrays) != set(merged):
            sys.exit(f"error: the {abi} libs.xml names different arrays than the {abis[0]} one")
        for name, items in arrays.items():
            merged[name].extend(items)
    lines = []
    for line in template:
        if isinstance(line, ArrayItems):
            lines.extend(merged[line.name])
        else:
            lines.append(line)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8")


def extract_native_libraries(apk, abi, project):
    prefix = f"lib/{abi}/"
    destination = project / "libs" / abi
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(apk) as archive:
        for name in archive.namelist():
            if not name.startswith(prefix) or name.endswith("/"):
                continue
            (destination / pathlib.PurePosixPath(name).name).write_bytes(archive.read(name))


def set_target_abis(project, abis):
    properties = project / GRADLE_PROPERTIES
    text = properties.read_text(encoding="utf-8")
    text, count = TARGET_ABI_LIST.subn("qtTargetAbiList=" + ",".join(abis), text)
    if count != 1:
        sys.exit("error: gradle.properties does not name qtTargetAbiList exactly once")
    properties.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, help="directory to assemble the project in")
    parser.add_argument("--inputs", required=True, help="directory holding one project per ABI")
    parser.add_argument("apks", nargs="+", help="the per-ABI APKs to take native libraries from")
    args = parser.parse_args()

    inputs = pathlib.Path(args.inputs)
    output = pathlib.Path(args.output)
    apks = [pathlib.Path(apk) for apk in args.apks]

    abis = []
    for apk in apks:
        abi = apk_abi(apk)
        if abi in abis:
            sys.exit(f"error: two inputs carry {abi}")
        abis.append(abi)

    projects = {}
    for abi in abis:
        project = inputs / abi
        if not (project / "build.gradle").is_file():
            sys.exit(f"error: no Android project for {abi} under {inputs}")
        projects[abi] = project

    for abi in abis[1:]:
        compare_projects(projects[abis[0]], projects[abi], abis[0], abi)

    if output.exists():
        shutil.rmtree(output)
    shutil.copytree(projects[abis[0]], output, symlinks=True)
    union_libs_xml(projects, abis, output / LIBS_XML)
    for apk, abi in zip(apks, abis):
        extract_native_libraries(apk, abi, output)
    set_target_abis(output, abis)
    # Uploading a build tree loses the executable bit, and the wrapper is how
    # Gradle is run.
    (output / "gradlew").chmod(0o755)
    print(f"universal Android project for {', '.join(abis)} in {output}")


if __name__ == "__main__":
    main()
