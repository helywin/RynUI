#!/usr/bin/env python3
"""Run finite native component-page acceptance; Linux always uses private Wayland."""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "tests/fixtures/gallery-page-acceptance.schema.json"


def validate_result(value, expected_platform, preset, configuration):
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or set(value) != set(schema["required"]):
        raise ValueError("Native result fields do not match the page acceptance schema")
    types = {"string": str, "boolean": bool, "integer": int}
    for key, rule in schema["properties"].items():
        item = value[key]
        if type(item) is not types[rule["type"]]:
            raise ValueError(f"Wrong result type: {key}")
        if "const" in rule and item != rule["const"]:
            raise ValueError(f"Wrong result version: {key}")
        if "enum" in rule and item not in rule["enum"]:
            raise ValueError(f"Wrong result value: {key}")
        if "minimum" in rule and item < rule["minimum"]:
            raise ValueError(f"Negative result count: {key}")
        if "minLength" in rule and len(item) < rule["minLength"]:
            raise ValueError(f"Empty result field: {key}")
    if (value["platform"], value["preset"], value["configuration"]) != (expected_platform, preset, configuration):
        raise ValueError("Native platform/preset/configuration differs from requested build")
    if not value["passed"] or value["error"] or value["cases"] < 1 or value["checks"] < 1:
        raise ValueError("Native checks did not pass")
    if value["submitted_frames"] < value["cases"] or not value["shader"] or not value["font_source"]:
        raise ValueError("Native rendering/font evidence is incomplete")
    backend = value["backend"].lower()
    if expected_platform == "linux" and backend != "vulkan":
        raise ValueError("Linux acceptance requires actual Vulkan")
    if expected_platform == "windows" and (backend not in ("direct3d12", "d3d12") or value["toolchain"] != "MSVC"):
        raise ValueError("Windows acceptance requires actual MSVC/D3D12")
    if not value["system_fonts"]:
        raise ValueError("No system font face was exercised")
    return value


def run(binary, log_dir, preset, configuration="Release", timeout=30, pages=(), execute=subprocess.run, system=None,
        visual=False, i18n=False):
    binary = Path(binary).resolve()
    log_dir = Path(log_dir).resolve()
    if not log_dir.is_relative_to(ROOT / "out") or log_dir == ROOT / "out":
        raise ValueError("Acceptance output must be beneath this repository's out directory")
    if not binary.is_file() or not 1 <= timeout <= 120:
        raise ValueError("A built Gallery binary and a timeout in 1..120 seconds are required")
    native_platform = {"Linux": "linux", "Windows": "windows"}.get(system or platform.system())
    if not native_platform:
        raise ValueError("This native acceptance supports Linux and Windows")
    log_dir.mkdir(parents=True, exist_ok=True)
    output = log_dir / "native-result.json"
    output.unlink(missing_ok=True)
    command = [str(binary), "--page-acceptance", f"--page-output={output}", f"--page-timeout={timeout}"]
    if visual or i18n:
        command.append("--page-visual")
    if i18n:
        command.append("--page-i18n")
    command += [f"--page={page}" for page in pages]
    if native_platform == "linux":
        command = [sys.executable, str(ROOT / "tools/run_background_wayland.py"), "--log-dir", str(log_dir / "wayland"),
                   "--timeout", str(timeout), "--", *command]
    summary = {"platform": native_platform, "preset": preset, "configuration": configuration, "status": "failed",
               "return_code": None, "error": "", "desktop_fallback": False}
    try:
        completed = execute(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout + 20)
        summary["return_code"] = completed.returncode
        (log_dir / "native.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
        if completed.returncode == 124:
            summary["status"] = "timeout"
            raise ValueError("Private native test timed out; no desktop fallback was started")
        if completed.returncode:
            raise ValueError(f"Native test exited with {completed.returncode}")
        value = validate_result(json.loads(output.read_text(encoding="utf-8")), native_platform, preset, configuration)
        if not pages and not (visual or i18n) and not (value["host_input_area"] and value["host_cursor_sync"]):
            raise ValueError("Default matrix did not exercise host input area/cursor synchronization")
        summary["status"] = "passed"
    except subprocess.TimeoutExpired:
        summary["status"] = "timeout"
        summary["error"] = "Native runner deadline exceeded; no desktop fallback was started"
    except (ValueError, OSError, json.JSONDecodeError) as error:
        summary["error"] = str(error)
    (log_dir / "runner-result.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0 if summary["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--log-dir", type=Path, default=ROOT / "out/gallery-pages/acceptance")
    parser.add_argument("--preset", required=True)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--timeout", type=int, default=30)
    parser.add_argument("--page", action="append", default=[])
    parser.add_argument("--visual", action="store_true", help="Use the hidden offscreen native path")
    parser.add_argument("--i18n", action="store_true", help="Run the bounded bilingual sequence (implies --visual)")
    args = parser.parse_args()
    try:
        return run(args.binary, args.log_dir, args.preset, args.configuration, args.timeout, args.page,
                   visual=args.visual, i18n=args.i18n)
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    sys.exit(main())
