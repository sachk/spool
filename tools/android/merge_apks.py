#!/usr/bin/env python3
"""Union the lib/ trees of several per-ABI APKs into one unsigned APK.

Every input is the same application compiled for a different architecture, so
everything outside lib/ has to be byte-identical between them. That is checked
rather than assumed: if two matrix legs disagree about the manifest, the
resources or the version code, merging them would quietly keep whichever was
read first and ship an APK that matches neither.

Each entry keeps the compression it arrived with. That matters more than it
looks: the manifest leaves extractNativeLibs at its default, so the platform
unpacks native libraries at install time and Qt accordingly deflates them --
101 MiB of libraries ship as 36 MiB. Storing them instead, to make them
mappable in place, would nearly triple the download for a file whose whole
purpose is to be the one a stranger can grab. resources.arsc arrives stored,
as the platform requires, and stays that way.
"""

import argparse
import sys
import zipfile

# Rewritten by the signer, so a difference here is not a difference in content.
SIGNATURE_PREFIX = "META-INF/"
SIGNATURE_SUFFIXES = (".SF", ".RSA", ".DSA", ".EC")
MANIFEST_ENTRY = "META-INF/MANIFEST.MF"

def is_signature(name):
    if name == MANIFEST_ENTRY:
        return True
    return name.startswith(SIGNATURE_PREFIX) and name.endswith(SIGNATURE_SUFFIXES)


def is_native(name):
    return name.startswith("lib/")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("inputs", nargs="+")
    args = parser.parse_args()

    # The first input supplies the shared payload; the rest contribute only
    # their own architecture.
    shared = {}
    native = {}
    abis = []
    for index, path in enumerate(args.inputs):
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if info.is_dir() or is_signature(info.filename):
                    continue
                entry = (archive.read(info.filename), info.compress_type)
                if is_native(info.filename):
                    parts = info.filename.split("/")
                    if len(parts) > 2 and parts[1] not in abis:
                        abis.append(parts[1])
                    native.setdefault(info.filename, entry)
                    continue
                if index == 0:
                    shared[info.filename] = entry
                elif info.filename not in shared:
                    sys.exit(
                        f"error: {path} carries {info.filename}, which "
                        f"{args.inputs[0]} does not"
                    )
                elif shared[info.filename][0] != entry[0]:
                    sys.exit(
                        f"error: {info.filename} differs between "
                        f"{args.inputs[0]} and {path}; the inputs are not the "
                        "same build"
                    )

    if not native:
        sys.exit("error: no native libraries found in any input")

    with zipfile.ZipFile(args.output, "w") as out:
        for name, (payload, method) in sorted(shared.items()):
            out.writestr(name, payload, compress_type=method)
        for name, (payload, method) in sorted(native.items()):
            out.writestr(name, payload, compress_type=method)

    print(f"merged {len(args.inputs)} APKs covering {', '.join(abis)}")
    print(f"  shared entries: {len(shared)}")
    print(f"  native entries: {len(native)}")


if __name__ == "__main__":
    main()
