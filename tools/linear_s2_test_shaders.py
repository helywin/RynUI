#!/usr/bin/env python3
"""Fixed test-only Linear variants; identities stay in ignored build/out storage."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import effect_shader_closure as closure


def variants():
    result = {"auto": [], "wide": ["RYNUI_TEST_LINEAR_FORCE_WIDE=1"]}
    for mode in range(1, 8):
        result[f"probe{mode}"] = [f"RYNUI_TEST_LINEAR_PROBE={mode}"]
    for mode in range(1, 5):
        result[f"wide-probe{mode}"] = ["RYNUI_TEST_LINEAR_FORCE_WIDE=1", f"RYNUI_TEST_LINEAR_PROBE={mode}"]
    return result


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--identity-only", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    if output.is_relative_to(root) and not output.is_relative_to(root / "out"):
        parser.error("In-repository test artifacts must be under ignored out/")
    output.mkdir(parents=True, exist_ok=True)
    source = closure.manifest(root)
    pipeline = source["pipelines"]["rounded_effect_paint"]
    result = {"kind": "static recipes only" if args.identity_only else "compiled test variants",
              "config": args.config, "pipeline": pipeline, "files": source["files"], "variants": {}}
    if not args.identity_only:
        if args.compiler is None:
            parser.error("--compiler is required unless --identity-only")
        closure.validate(root, root / "shaders/rounded_effect.lock.yaml")
        result["compiler_sha256"] = sha(args.compiler.read_bytes())
    for name, extra in variants().items():
        defines = ["RYNUI_FRAGMENT_SHADER=1", *extra]
        entry = {"source_closure_sha256": pipeline["closure_sha256"], "defines": defines,
                 "stage": "fragment", "entry": "PSMain", "artifacts": {}}
        result["variants"][name] = entry
        recipe = {"config": args.config, "closure": pipeline["closure_sha256"], "defines": defines}
        entry["recipe_sha256"] = sha(json.dumps(recipe, sort_keys=True).encode())
        if args.identity_only:
            continue
        for destination, suffix in (("DXIL", "dxil"), ("SPIRV", "spv")):
            artifact = output / f"{name}.{suffix}"
            command = [str(args.compiler), str(root / pipeline["entry"]), "-s", "HLSL", "-d", destination,
                       "-t", "fragment", "-e", "PSMain", *[f"-D{d}" for d in defines],
                       "-I", str(root / "shaders"), "-o", str(artifact)]
            completed = subprocess.run(command, capture_output=True)
            (output / f"{name}.{suffix}.log").write_bytes(completed.stdout + completed.stderr)
            completed.check_returncode()
            entry["artifacts"][suffix] = {"sha256": sha(artifact.read_bytes()), "bytes": artifact.stat().st_size,
                                          "command": command}
        reflected = output / f"{name}.json"
        command = [str(args.compiler), str(output / f"{name}.spv"), "-s", "SPIRV", "-d", "JSON",
                   "-t", "fragment", "-e", "PSMain", "-o", str(reflected)]
        completed = subprocess.run(command, capture_output=True)
        (output / f"{name}.reflection.log").write_bytes(completed.stdout + completed.stderr)
        completed.check_returncode()
        reflection = json.loads(reflected.read_text(encoding="utf-8"))
        if reflection.get("storage_buffers") != 2:
            raise RuntimeError(f"{name}: test fragment lost the actual two-buffer binding")
        inputs = [(item.get("location"), item.get("type")) for item in reflection.get("inputs", [])]
        expected_inputs = [(index, "float4" if index < 6 else "uint4") for index in range(7)]
        if sorted(inputs) != expected_inputs:
            raise RuntimeError(f"{name}: test fragment lost the production seven-input signature: {inputs}")
        entry["artifacts"]["reflection"] = {"sha256": sha(reflected.read_bytes()), "bytes": reflected.stat().st_size}
    (output / "manifest.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
