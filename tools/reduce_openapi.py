#!/usr/bin/env python3

import json
import sys
from copy import deepcopy


def usage() -> int:
    print(
        "usage: reduce_openapi.py <input.json> <output.json> <path> [<path> ...]",
        file=sys.stderr,
    )
    return 1


def collect_refs(value, refs):
    if isinstance(value, dict):
        ref = value.get("$ref")
        if isinstance(ref, str) and ref.startswith("#/components/"):
            refs.add(ref)
        for nested in value.values():
            collect_refs(nested, refs)
    elif isinstance(value, list):
        for nested in value:
            collect_refs(nested, refs)


def main() -> int:
    if len(sys.argv) < 4:
        return usage()

    input_path = sys.argv[1]
    output_path = sys.argv[2]
    selected_paths = sys.argv[3:]

    with open(input_path, "r", encoding="utf-8") as handle:
        spec = json.load(handle)

    source_paths = spec.get("paths", {})
    missing_paths = [path for path in selected_paths if path not in source_paths]
    if missing_paths:
        print(f"error: missing OpenAPI paths: {', '.join(missing_paths)}", file=sys.stderr)
        return 2

    reduced_paths = {path: deepcopy(source_paths[path]) for path in selected_paths}
    refs = set()
    for path_item in reduced_paths.values():
        collect_refs(path_item, refs)

    source_components = spec.get("components", {})
    reduced_components = {}
    seen_refs = set()

    while refs:
        ref = refs.pop()
        if ref in seen_refs:
            continue
        seen_refs.add(ref)

        parts = ref.split("/")
        if len(parts) != 4:
            continue

        _, marker, section, name = parts
        if marker != "components":
            continue

        section_data = source_components.get(section, {})
        if name not in section_data:
            continue

        reduced_components.setdefault(section, {})[name] = deepcopy(section_data[name])
        collect_refs(section_data[name], refs)

    if "securitySchemes" in source_components:
        reduced_components["securitySchemes"] = deepcopy(source_components["securitySchemes"])

    selected_tags = set()
    for path_item in reduced_paths.values():
        for operation in path_item.values():
            for tag in operation.get("tags", []):
                selected_tags.add(tag)

    reduced_spec = {
        "openapi": spec.get("openapi"),
        "info": deepcopy(spec.get("info", {})),
        "servers": deepcopy(spec.get("servers", [])),
        "paths": reduced_paths,
        "components": reduced_components,
    }

    if "jsonSchemaDialect" in spec:
        reduced_spec["jsonSchemaDialect"] = spec["jsonSchemaDialect"]
    if "tags" in spec:
        reduced_spec["tags"] = [tag for tag in spec["tags"] if tag.get("name") in selected_tags]

    with open(output_path, "w", encoding="utf-8") as handle:
        json.dump(reduced_spec, handle, indent=2)
        handle.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
