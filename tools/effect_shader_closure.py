#!/usr/bin/env python3
"""Lock/check the exact LF-normalized effect shader include closures.

The lock is JSON (a YAML subset), so no third-party parser is needed.
Only --write-lock mutates the lock; shader compilation never updates it.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile


ENTRIES = ("rounded_effect", "rounded_effect_paint")


def closure(root: Path, entry: str) -> list[str]:
    shader_root = (root / "shaders").resolve()
    visited: set[Path] = set()
    visiting: set[Path] = set()

    def visit(path: Path) -> None:
        path = path.resolve()
        if not path.is_relative_to(shader_root):
            raise ValueError(f"Shader include escapes shaders/: {path}")
        if path in visiting:
            raise ValueError(f"Shader include cycle: {path}")
        if path in visited:
            return
        visiting.add(path)
        source = path.read_text(encoding="utf-8")
        for line in source.splitlines():
            if re.match(r"\s*#\s*include\b", line):
                match = re.fullmatch(r'\s*#\s*include\s+"([^"\n]+)"\s*(?://.*)?', line)
                if not match:
                    raise ValueError(f"Only fixed quoted shader includes are allowed: {path}: {line}")
                visit(path.parent / match[1])
        visiting.remove(path)
        visited.add(path)

    visit(shader_root / (entry + ".hlsl"))
    return sorted(path.relative_to(root.resolve()).as_posix() for path in visited)


def manifest(root: Path) -> dict:
    pipelines = {}
    files = {}
    for entry in ENTRIES:
        sources = closure(root, entry)
        for name in sources:
            content = (root / name).read_bytes().replace(b"\r\n", b"\n")
            files[name] = {"sha256": hashlib.sha256(content).hexdigest(), "bytes": len(content)}
        records = "".join(name + "\0" + files[name]["sha256"] + "\n" for name in sources).encode()
        pipelines[entry] = {
            "entry": "shaders/" + entry + ".hlsl",
            "closure": sources,
            "closure_sha256": hashlib.sha256(records).hexdigest(),
            "vertex_inputs": ["float4"] * 7 + (["uint4"] if entry != "rounded_effect" else []),
            "fragment_storage_buffers": 2 if entry != "rounded_effect" else 0,
            "stage_defines": {"vertex": ["RYNUI_VERTEX_SHADER=1"],
                              "fragment": ["RYNUI_FRAGMENT_SHADER=1"]},
            "artifacts": [entry + "." + stage + "." + suffix
                          for stage in ("vertex", "fragment") for suffix in ("dxil", "spv", "json")],
        }
    return {"schema_version": 2, "hash_normalization": "lf", "instance_size_bytes": 112,
            "effect_paint_abi": 1, "link_stride": 16, "field_stride": 32, "stop_stride": 32,
            "cap_geometry_abi": 1, "cap_geometry_stride": 32,
            "entry_points": {"vertex": "VSMain", "fragment": "PSMain"},
            "files": dict(sorted(files.items())), "pipelines": pipelines}


def validate(root: Path, lock: Path) -> dict:
    expected = manifest(root)
    try:
        actual = json.loads(lock.read_text(encoding="utf-8"))
    except (ValueError, OSError) as error:
        raise ValueError("Effect shader lock needs explicit regeneration: run "
                         "python tools/effect_shader_closure.py --write-lock") from error
    if actual != expected:
        changed = [name for name, info in expected["files"].items() if actual.get("files", {}).get(name) != info]
        raise ValueError("Effect shader closure lock drift: " + ", ".join(changed or ["schema/closure/ABI"]))
    return expected


def negative_test(root: Path, lock: Path, output: Path) -> None:
    validate(root, lock)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="closure-", dir=output) as directory:
        test_root = Path(directory)
        shutil.copytree(root / "shaders", test_root / "shaders")
        test_lock = test_root / "shaders/rounded_effect.lock.yaml"
        shutil.copyfile(lock, test_lock)
        changed_recipe = json.loads(test_lock.read_text(encoding="utf-8"))
        changed_recipe["pipelines"]["rounded_effect_paint"]["stage_defines"]["vertex"] = []
        test_lock.write_text(json.dumps(changed_recipe), encoding="utf-8")
        try:
            validate(test_root, test_lock)
        except ValueError:
            pass
        else:
            raise ValueError("Stage-define mutation incorrectly passed the shader lock")
        shutil.copyfile(lock, test_lock)
        include = test_root / "shaders/rounded_effect_coverage.hlsli"
        include.write_bytes(include.read_bytes() + b"\n// negative include-only mutation\n")
        try:
            validate(test_root, test_lock)
        except ValueError:
            pass
        else:
            raise ValueError("Include-only mutation incorrectly passed the closure lock")
        include.write_text('#include "rounded_effect.hlsl"\n')
        try:
            manifest(test_root)
        except ValueError:
            pass
        else:
            raise ValueError("Include cycle was accepted")
        include.write_text('#include "../outside.hlsli"\n')
        try:
            manifest(test_root)
        except ValueError:
            pass
        else:
            raise ValueError("Escaping include was accepted")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--write-lock", action="store_true")
    parser.add_argument("--entry", choices=ENTRIES)
    parser.add_argument("--stage", choices=("vertex", "fragment"))
    parser.add_argument("--define", action="append", default=[])
    parser.add_argument("--dependency", type=Path, action="append", default=[])
    parser.add_argument("--negative-test", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    lock = args.lock or root / "shaders/rounded_effect.lock.yaml"
    if args.write_lock:
        lock.write_text(json.dumps(manifest(root), indent=2) + "\n", encoding="utf-8")
    expected = validate(root, lock)
    if args.stage:
        if not args.entry:
            parser.error("--stage requires --entry")
        if args.define != expected["pipelines"][args.entry]["stage_defines"][args.stage]:
            raise ValueError("CMake stage defines differ from the locked shader recipe")
    elif args.define:
        parser.error("--define requires --stage")
    if args.dependency:
        if not args.entry:
            parser.error("--dependency requires --entry")
        actual = sorted(path.resolve().relative_to(root).as_posix() for path in args.dependency)
        if actual != expected["pipelines"][args.entry]["closure"]:
            raise ValueError("CMake DEPENDS differs from the locked source closure: " + args.entry)
    if args.negative_test:
        negative_test(root, lock, args.negative_test)
    for entry, info in expected["pipelines"].items():
        print(entry + " closure_sha256=" + info["closure_sha256"])


if __name__ == "__main__":
    main()
