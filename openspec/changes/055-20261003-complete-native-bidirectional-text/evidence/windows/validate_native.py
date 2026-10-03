"""Run Windows bidi windows and verify EXE/log/GPU readback identities."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {
    "initial", "selection", "rtl-start", "rtl-left", "direction-rtl", "direction-ltr",
    "preedit", "commit", "undo", "mask", "wrapped-area", "area-selection", "dark-compact",
    "resized-window", "pointer-before", "pointer-hit", "popup", "popup-closed", "inactive",
}
RESIZED = {"resized-window", "pointer-before", "pointer-hit", "popup", "popup-closed", "inactive"}
EXPECTED = (
    "bidi_acceptance=passed", "gpu_driver=direct3d12", "shader_format=DXIL",
    "inputs=7 content_runs=1 pointer_events=3", "submits=38",
    "system_fonts=passed arabic_hebrew=passed visual_navigation=passed disjoint_selection=passed",
    "direction=passed clipboard=passed history=passed preedit=passed mask=passed ime_area=passed",
    "window_resize=1420x1000 pointer_hit=passed popup=passed idle_polls=3 deadline=none disposed=1 exit_code=0",
)
DIFFERENT = (
    ("initial", "selection"), ("rtl-start", "rtl-left"), ("direction-rtl", "direction-ltr"),
    ("preedit", "commit"), ("wrapped-area", "area-selection"),
    ("pointer-before", "pointer-hit"), ("popup", "popup-closed"),
)


def digest(path):
    data = path.read_text(encoding="utf-8").encode("utf-8") if path.suffix == ".log" else path.read_bytes()
    return hashlib.sha256(data).hexdigest()


def validate_log(log, requested_scale):
    if any(item not in log for item in EXPECTED):
        raise RuntimeError("Native bidi contract missing: " + log)
    for name in ("native_starts", "native_areas"):
        match = re.search(r"\b" + name + r"=(\d+)", log)
        if not match or int(match.group(1)) <= 0:
            raise RuntimeError("SDL native port was not exercised")
    actual = float(re.search(r"\brender_scale=([\d.]+)", log).group(1))
    system = float(re.search(r"\bsystem_display_scale=([\d.]+)", log).group(1))
    if abs(actual - (system if requested_scale is None else float(requested_scale))) > .001:
        raise RuntimeError("Requested/system render scale differs")


def verify_saved_runs():
    runs = json.loads((DEST / "runs.json").read_text(encoding="utf-8"))
    expected = {(configuration, mode) for configuration in ("Debug", "Release")
                for mode in ("system", "scale-1", "scale-1.25", "scale-1.5", "scale-2")}
    if len(runs) != 10 or {(run["configuration"], run["mode"]) for run in runs} != expected:
        raise RuntimeError("Ten distinct Windows runs required")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("EXE changed after capture")
        log = DEST / run["log"]
        if digest(log) != run["log_sha256"]:
            raise RuntimeError("Native log changed")
        validate_log(log.read_text(encoding="utf-8"), run["requested_scale"])
        if len(run["captures"]) != len(NAMES) or {Path(item["path"]).stem for item in run["captures"]} != NAMES:
            raise RuntimeError("GPU inventory changed")
        hashes = {}
        for capture in run["captures"]:
            path = DEST / capture["path"]
            extent = (1420, 1000) if path.stem in RESIZED else (1600, 1100)
            if digest(path) != capture["sha256"] or capture["size"] != list(extent):
                raise RuntimeError("GPU readback identity changed")
            with Image.open(path) as png:
                if png.size != extent:
                    raise RuntimeError("GPU dimensions changed")
            hashes[path.stem] = capture["sha256"]
        if any(hashes[left] == hashes[right] for left, right in DIFFERENT):
            raise RuntimeError("Distinct native states have identical readbacks")
    print("All saved EXE, native log, dimensions and GPU readback identities match", flush=True)


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/055-bidi-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--bidi-acceptance", "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            log_path = DEST / (identity + ".log")
            log_path.write_text(log, encoding="utf-8")
            if result.returncode or before != digest(executable):
                raise RuntimeError(identity + " failed: " + log)
            validate_log(log, scale)
            bitmaps = {path.stem: path for path in raw.glob("*.bmp")}
            if set(bitmaps) != NAMES:
                raise RuntimeError("Unexpected GPU inventory")
            captures = []
            for name in sorted(NAMES):
                target = output / (name + ".png")
                with Image.open(bitmaps[name]) as bitmap:
                    bitmap.convert("RGBA").save(target)
                with Image.open(target) as png:
                    size = list(png.size)
                captures.append({"path": target.relative_to(DEST).as_posix(), "size": size, "sha256": digest(target)})
            runs.append({"configuration": configuration, "mode": mode, "requested_scale": scale,
                         "arguments": arguments, "executable": executable.relative_to(ROOT).as_posix(),
                         "executable_sha256": before, "log": log_path.name, "log_sha256": digest(log_path),
                         "exit_code": result.returncode, "captures": captures})
            print(identity + f": passed; {len(NAMES)} GPU readbacks", flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    verify_saved_runs()
    print(f"Validated {len(runs)} Windows runs and {len(runs) * len(NAMES)} GPU readbacks", flush=True)


if __name__ == "__main__":
    if "--verify-only" in sys.argv:
        verify_saved_runs()
    else:
        main()
