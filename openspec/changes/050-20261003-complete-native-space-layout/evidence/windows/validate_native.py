"""Run native Space windows and verify executable and GPU readback identities."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {"initial", "hover", "focus", "rtl", "small", "large", "vertical-rtl", "mixed-status",
         "filled-error", "dark", "unicode-edit", "popup-open", "popup-edit", "popup-closed", "pointer-hit",
         "nested-single", "resized", "inactive"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_saved_runs():
    runs = json.loads((DEST / "runs.json").read_text(encoding="utf-8"))
    if len(runs) != 10:
        raise RuntimeError("Expected ten Windows runs")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("EXE changed after evidence capture: " + run["mode"])
        if len(run["captures"]) != len(NAMES):
            raise RuntimeError("Readback inventory changed")
        for capture in run["captures"]:
            if digest(DEST / capture["path"]) != capture["sha256"]:
                raise RuntimeError("Readback changed: " + capture["path"])
    print("All saved EXE and readback identities match", flush=True)


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/050-space-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--space-acceptance", "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            (DEST / (identity + ".log")).write_text(log, encoding="utf-8")
            expected = ("space_acceptance=passed", "gpu_driver=direct3d12", "shader_format=DXIL",
                        "pointer_events=6 text_events=2 clicks=1 submits=36", "content_runs=1 label_runs=13",
                        "baselines=matched editor_identity=retained popup_cleanup=passed",
                        "resize=1420x900 idle_polls=3 deadline=none disposed=1")
            if result.returncode or any(value not in log for value in expected) or before != digest(executable):
                raise RuntimeError(identity + " failed: " + log)
            bitmaps = {path.stem: path for path in raw.glob("*.bmp")}
            if set(bitmaps) != NAMES:
                raise RuntimeError(identity + " unexpected readback inventory")
            captures = []
            for name in sorted(NAMES):
                target = output / (name + ".png")
                with Image.open(bitmaps[name]) as bitmap:
                    bitmap.convert("RGBA").save(target)
                expected_size = (1420, 900) if name in {"resized", "inactive"} else (1600, 1000)
                with Image.open(target) as png:
                    if png.size != expected_size:
                        raise RuntimeError(identity + ": invalid GPU extent " + name)
                captures.append({"path": target.relative_to(DEST).as_posix(), "size": list(expected_size),
                                 "sha256": digest(target)})
            for left, right in (("initial", "rtl"), ("small", "large"), ("large", "vertical-rtl"),
                                ("filled-error", "dark"), ("popup-open", "popup-closed"),
                                ("pointer-hit", "nested-single")):
                if digest(output / (left + ".png")) == digest(output / (right + ".png")):
                    raise RuntimeError(identity + ": distinct states have identical readbacks")
            runs.append({"configuration": configuration, "mode": mode, "arguments": arguments,
                         "executable": executable.relative_to(ROOT).as_posix(), "executable_sha256": before,
                         "exit_code": result.returncode, "captures": captures})
            print(identity + f": passed; {len(NAMES)} GPU readbacks", flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    verify_saved_runs()
    print(f"Validated {len(runs)} actual Windows runs and {len(runs) * len(NAMES)} GPU readbacks", flush=True)


if __name__ == "__main__":
    if "--verify-only" in sys.argv:
        verify_saved_runs()
    else:
        main()
