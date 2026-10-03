"""Validate actual Windows D3D12 readback against the packed SDF reference."""
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {"coverage-light", "coverage-dark", "textarea-light", "textarea-dark",
         "wave-restart", "reduced", "inactive", "resized"}
NAMES.update(f"wave-{theme}-{time}" for theme in ("light", "dark") for time in (0, 100, 400, 1000, 2000))
TOLERANCE = 2  # UNORM rounding and shader float arithmetic, in 8-bit channel values.


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_pixels(directory):
    checks = []
    for theme in ("light", "dark"):
        stem = "coverage-" + theme
        pixels = (directory / (stem + ".rgb")).read_bytes()
        with (directory / (stem + ".csv")).open(newline="", encoding="utf-8") as stream:
            regions = list(csv.DictReader(stream))
        if len(regions) != 22 or {row["name"] for row in regions if row["name"].startswith("corners-")} != {
                "corners-" + str(mask) for mask in range(16)}:
            raise RuntimeError("All sixteen corner combinations must be checked")
        with Image.open(directory / (stem + ".png")) as image:
            image = image.convert("RGB")
            for region in regions:
                x, y, width, height, offset, partial = (int(region[key]) for key in
                    ("x", "y", "width", "height", "offset", "partial"))
                reference = Image.frombytes("RGB", (width, height), pixels[offset:offset + width * height * 3])
                actual = image.crop((x, y, x + width, y + height))
                difference = ImageChops.difference(actual, reference)
                maximum = max(high for low, high in difference.getextrema())
                if maximum > TOLERANCE:
                    raise RuntimeError(f"{directory.name}/{stem}/{region['name']}: GPU error {maximum} > {TOLERANCE}; "
                                       f"mismatch bounds={difference.getbbox()}")
                if partial < 4:
                    raise RuntimeError("No fractional boundary coverage in " + region["name"])
                checks.append({"capture": stem, "case": region["name"], "pixels": width * height,
                               "maximum_channel_error": maximum, "fractional_reference_pixels": partial})
    return checks


def validate_log(log, requested):
    for expected in ("rounded_acceptance=passed gpu_driver=direct3d12 shader_format=DXIL",
                     "corner_masks=16 coverage_cases=22", "content_runs=1",
                     "wave_times=0,100,400,1000,2000 resize=1420x1000 reduced=passed inactive=passed idle_polls=3",
                     "deadline=none disposed=1 exit_code=0"):
        if expected not in log:
            raise RuntimeError("Missing native contract: " + log)
    scale = float(re.search(r"\brender_scale=([\d.]+)", log).group(1))
    system = float(re.search(r"\bsystem_display_scale=([\d.]+)", log).group(1))
    if abs(scale - (system if requested is None else float(requested))) > .001:
        raise RuntimeError("Requested/system scale mismatch")
    return scale


def verify_saved_runs():
    runs = json.loads((DEST / "runs.json").read_text(encoding="utf-8"))
    matrix = {(config, mode) for config in ("Debug", "Release") for mode in
              ("system", "scale-1", "scale-1.25", "scale-1.5", "scale-2")}
    if len(runs) != 10 or {(run["configuration"], run["mode"]) for run in runs} != matrix:
        raise RuntimeError("Incomplete Debug/Release DPI matrix")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("EXE changed after acceptance")
        directory = DEST / run["id"]
        if digest(DEST / run["log"]) != run["log_sha256"]:
            raise RuntimeError("Log changed after acceptance")
        validate_log((DEST / run["log"]).read_text(encoding="utf-8"), run["requested_scale"])
        for entry in run["files"]:
            if digest(directory / entry["path"]) != entry["sha256"]:
                raise RuntimeError("Readback/reference identity changed")
            if entry["path"].endswith(".png"):
                expected = (1420, 1000) if entry["path"] == "resized.png" else (1600, 1100)
                with Image.open(directory / entry["path"]) as image:
                    if image.size != expected or entry["size"] != list(expected):
                        raise RuntimeError("Readback dimensions changed")
        if {Path(entry["path"]).stem for entry in run["files"] if entry["path"].endswith(".png")} != NAMES:
            raise RuntimeError("Saved readback inventory changed")
        if validate_pixels(directory) != run["pixel_checks"]:
            raise RuntimeError("GPU pixel checks changed")
    auxiliary_path = DEST / "native-checks.json"
    if auxiliary_path.exists():
        auxiliary = json.loads(auxiliary_path.read_text(encoding="utf-8"))
        for entry in auxiliary["gallery_executables"]:
            if digest(ROOT / entry["path"]) != entry["sha256"]:
                raise RuntimeError("Gallery EXE changed after native smoke")
        for entry in auxiliary["logs"]:
            path = DEST / entry["path"]
            if digest(path) != entry["sha256"]:
                raise RuntimeError("Native smoke/shader log changed")
            log = path.read_text(encoding="utf-8")
            for fragment in entry["required_fragments"]:
                if fragment not in log:
                    raise RuntimeError("Native smoke/shader contract missing")
    print("Verified 10 EXEs/logs/capture sets and 440 GPU regions against SDF references", flush=True)


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_rounded_acceptance.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/056-rounded-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            directory = DEST / identity
            directory.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            log_path = DEST / (identity + ".log")
            log_path.write_text(log, encoding="utf-8", newline="\n")
            if result.returncode or before != digest(executable):
                raise RuntimeError(identity + " failed: " + log)
            actual_scale = validate_log(log, scale)
            if {path.stem for path in raw.glob("*.bmp")} != NAMES:
                raise RuntimeError("GPU capture inventory differs")
            for name in sorted(NAMES):
                with Image.open(raw / (name + ".bmp")) as bitmap:
                    bitmap.convert("RGBA").save(directory / (name + ".png"))
            for path in raw.glob("coverage-*.*"):
                if path.suffix in (".rgb", ".csv"):
                    if path.suffix == ".csv":
                        (directory / path.name).write_text(path.read_text(encoding="utf-8"),
                                                         encoding="utf-8", newline="\n")
                    else:
                        shutil.copyfile(path, directory / path.name)
            checks = validate_pixels(directory)
            for theme in ("light", "dark"):
                hashes = [digest(directory / f"wave-{theme}-{time}.png") for time in (0, 100, 400, 1000, 2000)]
                # Zero spread and completed fade both have no wave; those readbacks may match.
                if len(set(hashes[1:])) != len(hashes[1:]):
                    raise RuntimeError("Distinct wave times produced identical readbacks")
            files = []
            for path in sorted(directory.iterdir()):
                if path.is_file():
                    entry = {"path": path.name, "sha256": digest(path)}
                    if path.suffix == ".png":
                        with Image.open(path) as image:
                            entry["size"] = list(image.size)
                    files.append(entry)
            runs.append({"id": identity, "configuration": configuration, "mode": mode,
                         "requested_scale": scale, "render_scale": actual_scale,
                         "executable": executable.relative_to(ROOT).as_posix(), "executable_sha256": before,
                         "log": log_path.name, "log_sha256": digest(log_path), "files": files,
                         "pixel_tolerance": TOLERANCE, "pixel_checks": checks})
            print(f"{identity}: {len(NAMES)} GPU readbacks, {len(checks)} pixel regions passed", flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, indent=2) + "\n", encoding="utf-8", newline="\n")
    verify_saved_runs()
    checks = [check for run in runs for check in run["pixel_checks"]]
    print(f"GPU pixels={sum(check['pixels'] for check in checks)} "
          f"maximum_channel_error={max(check['maximum_channel_error'] for check in checks)} "
          f"captures={len(runs) * len(NAMES)}", flush=True)


if __name__ == "__main__":
    if "--verify-only" in sys.argv:
        verify_saved_runs()
    elif "--first-only" in sys.argv:
        directory = ROOT / "out/056-native-first"
        for name in ("coverage-light", "coverage-dark"):
            with Image.open(directory / (name + ".bmp")) as image:
                image.convert("RGB").save(directory / (name + ".png"))
        print(json.dumps(validate_pixels(directory), indent=2))
    else:
        main()
