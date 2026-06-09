#!/usr/bin/env python3

import argparse
import json
import re
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("template", type=Path)
    parser.add_argument("version_file", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    version = args.version_file.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit(f"invalid project version: {version}")

    appinfo = json.loads(args.template.read_text(encoding="utf-8"))
    appinfo["version"] = version
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(appinfo, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
