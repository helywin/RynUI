#!/usr/bin/env python3
"""Validate platform passed evidence against the current 6.6.5 source identity."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path

import check_ant_design_current_baseline as baseline


CHANGE = "012-20260922-upgrade-ant-design-reference-to-6-6-5"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_schema(value: object, schema: dict, location: str = "$" ) -> None:
    kind = schema.get("type")
    kinds = {"object": dict, "array": list, "string": str, "number": (int, float)}
    if kind and (not isinstance(value, kinds[kind]) or kind == "number" and isinstance(value, bool)):
        raise ValueError(f"{location}: expected {kind}")
    if "const" in schema and value != schema["const"]:
        raise ValueError(f"{location}: expected {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise ValueError(f"{location}: unsupported value")
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                raise ValueError(f"{location}.{key}: missing")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False and set(value) - set(properties):
            raise ValueError(f"{location}: unknown fields")
        for key, child in value.items():
            if key in properties:
                validate_schema(child, properties[key], f"{location}.{key}")
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise ValueError(f"{location}: too few items")
        for index, child in enumerate(value):
            validate_schema(child, schema.get("items", {}), f"{location}[{index}]")
    if isinstance(value, str):
        if len(value) < schema.get("minLength", 0):
            raise ValueError(f"{location}: empty string")
        if "pattern" in schema and re.fullmatch(schema["pattern"], value) is None:
            raise ValueError(f"{location}: invalid format")
    if kind == "number" and "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
        raise ValueError(f"{location}: out of range")


def expected(repo: Path) -> dict:
    token_lock = json.loads((repo / "design-tokens/ant-design/6.6.5/sources.lock.yaml").read_text(encoding="utf-8"))
    diff_path = repo / "openspec/changes" / CHANGE / "evidence/source-diff.json"
    generated = (repo / "examples/token_gallery/generated_ant_design_reference_catalog.inc").read_text(encoding="utf-8")
    match = re.search(r'kAntDesignReferenceCatalogHash = "([0-9a-f]{64})";', generated)
    if match is None:
        raise ValueError("generated Gallery SHA256 is missing")
    return {
        "schema_version": 1,
        "status": "passed",
        "release": {
            "version": "6.6.5", "tag": "6.6.5",
            "commit": "4a39f54842eade4e565ab336ef6097cd7e723cdd",
            "license": "MIT",
            "source_set_sha256": "cf9f304e57d439c3344fc7462fcd01f61f4bfc39cc337006d26312fa8001a2b3",
            "source_diff_sha256": sha256(diff_path),
        },
        "inventory": {
            "categories": 7, "components": 73, "tokens": 1198,
            "gallery_sha256": match.group(1),
            "token_catalog_sha256": token_lock["catalog"]["sha256"],
        },
    }


def within_evidence(repo: Path, path: str) -> Path:
    target_root = (repo / "openspec/changes" / CHANGE / "evidence").resolve()
    target = (target_root / path).resolve()
    if not target.is_relative_to(target_root) or "6.5.0" in path or "008-" in path:
        raise ValueError(f"old or external evidence path: {path}")
    return target


def validate(payload: dict, repo: Path, schema: dict, require_files: bool) -> None:
    validate_schema(payload, schema)
    current = expected(repo)
    for key in ("release", "inventory"):
        if payload[key] != current[key]:
            raise ValueError(f"{key}: evidence does not match the current 6.6.5 sources")
    environment = payload["environment"]
    if environment["platform"] == "windows":
        if environment["preset"] != "windows-msvc" or environment["window_system"] != "Win32" \
                or environment["shader_backend"] != "DXIL" or "MSVC" not in environment["compiler"]:
            raise ValueError("Windows evidence must identify MSVC/Win32/DXIL")
    elif environment["window_system"] != "Wayland" or environment["shader_backend"] != "SPIR-V" \
            or environment["preset"] not in ("linux-gcc", "linux-clang"):
        raise ValueError("Linux evidence must identify native Wayland/SPIR-V")
    paths = environment["screenshots"] + [environment["diagnostics"]]
    if len(paths) != len(set(paths)):
        raise ValueError("duplicate evidence path")
    for path in paths:
        target = within_evidence(repo, path)
        if require_files and not target.is_file():
            raise ValueError(f"evidence file does not exist: {path}")


def self_test(repo: Path, schema: dict) -> None:
    fixture = expected(repo)
    fixture["regressions"] = {key: "passed" for key in
        ("button", "input", "theme", "gallery", "generators", "benchmark", "dependencies")}
    fixture["environment"] = {
        "platform": "windows", "preset": "windows-msvc", "compiler": "MSVC 19.51",
        "window_system": "Win32", "gpu_driver": "test-driver", "shader_backend": "DXIL",
        "system_font": "test-font", "display_scale": 1.5,
        "screenshots": ["screenshots/fixture.png"], "diagnostics": "logs/fixture.json", "exit_code": 0,
    }
    validate(fixture, repo, schema, False)
    mutations = (
        ("status", "planning"),
        ("release.version", "6.5.0"),
        ("release.source_diff_sha256", "0" * 64),
        ("inventory.components", 72),
        ("regressions.gallery", "planning"),
        ("environment.preset", "windows-mingw"),
        ("environment.screenshots", ["../../008-old.png"]),
        ("environment.exit_code", 1),
    )
    for path, value in mutations:
        bad = copy.deepcopy(fixture)
        node = bad
        parts = path.split(".")
        for part in parts[:-1]:
            node = node[part]
        node[parts[-1]] = value
        try:
            validate(bad, repo, schema, False)
        except ValueError:
            continue
        raise AssertionError(f"invalid passed evidence accepted: {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    repo = args.repo_root.resolve()
    baseline.check(repo)
    schema = json.loads((repo / "openspec/changes" / CHANGE / "evidence/passed-evidence.schema.json").read_text(encoding="utf-8"))
    if args.self_test:
        self_test(repo, schema)
    if args.evidence:
        validate(json.loads(args.evidence.read_text(encoding="utf-8")), repo, schema, True)
    if not args.self_test and not args.evidence:
        parser.error("select --self-test or --evidence")
    print("Ant Design 6.6.5 evidence contract: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Ant Design evidence error: {error}", file=sys.stderr)
        sys.exit(1)
