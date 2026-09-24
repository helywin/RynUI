#!/usr/bin/env python3
"""Validate recorded Search platform-generic and Windows automation evidence."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path


CHANGE = "014-20260925-compose-search-from-input-and-button"
SCALES = ("1", "1.25", "1.5", "2")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def fields_from(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            break
        name, value = line.split("=", 1)
        require(name not in fields and bool(value), f"duplicate or empty field: {name}")
        fields[name] = value
    return fields


def validate(path: Path, scope: str) -> None:
    fields = fields_from(path)
    expected = {
        "schema_version": "1",
        "change": CHANGE,
        "scope": scope,
        "execution_platform": "windows",
        "build_system": "Ninja Multi-Config",
        "cpp_standard": "C++20",
        "source_version": "6.6.5",
        "dependency_mode": "BUNDLED",
        "source_contract_exit_code": "0",
        "search_component_exit_code": "0",
        "idle_benchmark_exit_code": "0",
        "git_diff_check_exit_code": "0",
        "exit_code": "0",
    }
    for name, value in expected.items():
        require(fields.get(name) == value, f"{path}: expected {name}={value}")
    require(fields.get("os", "").startswith("Microsoft Windows 11"), "Windows OS is unrecorded")
    require(fields.get("compiler", "").startswith("MSVC 19.51."), "MSVC x64 is unrecorded")
    require(re.fullmatch(r"[0-9a-f]{40}", fields.get("feature_commit_sha", "")) is not None,
            "feature commit SHA must be full")

    if scope == "platform-generic":
        for name, value in {
            "status": "passed", "preset": "windows-msvc-debug",
            "full_ctest_debug": "223/223", "idle_iterations": "10000",
            "idle_animation_updates": "0", "idle_scene_rebuilds": "0",
            "idle_material_updates": "0",
            "platform_behavior": "not-required-platform-generic",
        }.items():
            require(fields.get(name) == value, f"generic evidence: expected {name}={value}")
        return

    for name, value in {
        "status": "partial", "preset": "windows-msvc-debug",
        "window_system": "win32", "gpu_driver": "direct3d12",
        "shader_format": "DXIL", "font_source": "system",
        "host_display_scale": "1.5", "acceptance_scales": "1,1.25,1.5,2",
        "manual_visual_review": "passed", "native_ime": "pending",
    }.items():
        require(fields.get(name) == value, f"Windows automated evidence: expected {name}={value}")
    for scale in SCALES:
        image = path.parent / "screenshots" / f"windows-scale-{scale}.png"
        log = path.parent / "screenshots" / f"windows-scale-{scale}.txt"
        require(image.is_file() and image.stat().st_size > 20_000,
                f"missing Windows screenshot at scale {scale}")
        require(image.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n",
                f"invalid PNG screenshot at scale {scale}")
        hash_name = f"screenshot_{scale.replace('.', '_')}_sha256"
        require(fields.get(hash_name) == hashlib.sha256(image.read_bytes()).hexdigest(),
                f"Windows screenshot hash differs at scale {scale}")
        require(log.is_file(), f"missing Windows diagnostics at scale {scale}")
        diagnostics = log.read_text(encoding="utf-8")
        for stage in range(6):
            require(f"search_acceptance_stage={stage}" in diagnostics,
                    f"Windows scale {scale} missed stage {stage}")
        require(re.search(rf"(?:^|\s)display_scale={re.escape(scale)}(?:\s|$)",
                          diagnostics) is not None,
                f"Windows scale {scale} has wrong display scale")
        for marker in (
            "host_display_scale=1.5", "window_system=win32",
            "gpu_driver=direct3d12", "shader_format=DXIL", "font_source=system",
            "search_acceptance=true", "search_keyboard=true", "search_pointer=true",
            "search_blocked=true", "search_text=true", "search_scroll=true",
            "search_submits=3", "live_samples=27", "exit_code=0",
        ):
            require(marker in diagnostics, f"Windows scale {scale} lacks {marker}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scope", choices=("platform-generic", "windows-automated"),
                        required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    try:
        validate(args.evidence.resolve(), args.scope)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
