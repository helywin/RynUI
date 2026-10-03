"""Run actual Windows Input windows and verify saved binary/log/readback identities."""
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
    "initial", "dark", "compact", "variants-status", "soft-max", "count-hidden", "count-edit", "count-undo",
    "count-redo", "native-preedit", "native-hints", "clear-disabled", "clear-enabled", "clear-empty",
    "password-visible", "password-action-hidden", "search-submit", "search-clear", "resized", "resize-pointer",
    "popup", "popup-closed", "inactive",
}
RESIZED = {"resized", "resize-pointer", "popup", "popup-closed", "inactive"}
EXPECTED = (
    "input_acceptance=passed", "gpu_driver=direct3d12", "shader_format=DXIL",
    "pointer_events=16 clears=1 searches=2 clear_searches=1 submits=46 content_runs=1 inputs=12",
    "variants=4 count=passed history=passed hints=passed ime_stamp=passed actions=passed",
    "resize=1420x1000 popup=passed idle_polls=3 deadline=none disposed=1 exit_code=0",
)
DIFFERENT = (
    ("initial", "dark"), ("dark", "compact"), ("compact", "variants-status"), ("variants-status", "soft-max"),
    ("soft-max", "count-hidden"), ("count-hidden", "count-edit"), ("count-edit", "count-undo"),
    ("count-undo", "count-redo"), ("native-preedit", "native-hints"), ("clear-disabled", "clear-enabled"),
    ("clear-enabled", "clear-empty"), ("password-visible", "password-action-hidden"),
    ("search-submit", "search-clear"), ("popup", "popup-closed"),
)


def digest(path):
    data = path.read_text(encoding="utf-8").encode("utf-8") if path.suffix == ".log" else path.read_bytes()
    return hashlib.sha256(data).hexdigest()


def validate_log(log, expected_scale=None):
    if any(item not in log for item in EXPECTED):
        raise RuntimeError("Native acceptance contract is missing: " + log)
    for counter in ("native_starts", "native_areas"):
        match = re.search(r"\b" + counter + r"=(\d+)", log)
        if not match or int(match.group(1)) <= 0:
            raise RuntimeError("Real SDL input port was not exercised")
    actual = float(re.search(r"\brender_scale=([\d.]+)", log).group(1))
    system = float(re.search(r"\bsystem_display_scale=([\d.]+)", log).group(1))
    if abs(actual - (system if expected_scale is None else float(expected_scale))) > 0.001:
        raise RuntimeError("Requested/system render scale differs")


def verify_saved_runs():
    runs = json.loads((DEST / "runs.json").read_text(encoding="utf-8"))
    expected = {(configuration, mode) for configuration in ("Debug", "Release")
                for mode in ("system", "scale-1", "scale-1.25", "scale-1.5", "scale-2")}
    if {(run["configuration"], run["mode"]) for run in runs} != expected or len(runs) != 10:
        raise RuntimeError("Expected ten distinct Windows runs")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("EXE changed after capture: " + run["mode"])
        log = DEST / run["log"]
        if digest(log) != run["log_sha256"]:
            raise RuntimeError("Native log changed")
        validate_log(log.read_text(encoding="utf-8"), run["requested_scale"])
        if len(run["captures"]) != len(NAMES) or {Path(item["path"]).stem for item in run["captures"]} != NAMES:
            raise RuntimeError("GPU readback inventory changed")
        hashes = {}
        for capture in run["captures"]:
            path = DEST / capture["path"]
            name = path.stem
            extent = (1420, 1000) if name in RESIZED else (1600, 1100)
            if digest(path) != capture["sha256"] or capture["size"] != list(extent):
                raise RuntimeError("Readback identity changed: " + str(path))
            with Image.open(path) as png:
                if png.size != extent:
                    raise RuntimeError("Readback dimensions changed")
            hashes[name] = capture["sha256"]
        if any(hashes[left] == hashes[right] for left, right in DIFFERENT):
            raise RuntimeError("Distinct native states have identical GPU readbacks")
    print("All saved EXE, native log, dimensions and GPU readback identities match", flush=True)


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/052-input-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--input-features-acceptance", "--evidence-dir=" + str(raw)]
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
                raise RuntimeError(identity + ": unexpected readback inventory")
            captures = []
            for name in sorted(NAMES):
                target = output / (name + ".png")
                with Image.open(bitmaps[name]) as bitmap:
                    bitmap.convert("RGBA").save(target)
                extent = (1420, 1000) if name in RESIZED else (1600, 1100)
                with Image.open(target) as png:
                    if png.size != extent:
                        raise RuntimeError(identity + ": invalid GPU extent " + name)
                captures.append({"path": target.relative_to(DEST).as_posix(), "size": list(extent),
                                 "sha256": digest(target)})
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
