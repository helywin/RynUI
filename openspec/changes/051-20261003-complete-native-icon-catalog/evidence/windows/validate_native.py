"""Exercise actual Windows Icon windows and verify EXE/GPU-readback identities."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {"initial", "rotate-45", "rotate-90", "derived-colors", "dark-primary", "clip-on", "clip-off",
         "spin-first", "spin-second", "reduced-motion", "theme-motion-off", "source-four-layers", "hidden",
         "resized", "pointer-custom", "popup-spin", "popup-closed", "inactive"}
RESIZED = {"resized", "pointer-custom", "popup-spin", "popup-closed", "inactive"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_saved_runs():
    runs = json.loads((DEST / "runs.json").read_text(encoding="utf-8"))
    if len(runs) != 10:
        raise RuntimeError("Expected ten actual Windows runs")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("EXE changed after capture: " + run["mode"])
        if len(run["captures"]) != len(NAMES):
            raise RuntimeError("Readback inventory changed")
        for capture in run["captures"]:
            if digest(DEST / capture["path"]) != capture["sha256"]:
                raise RuntimeError("Readback changed: " + capture["path"])
    print("All saved EXE and GPU readback identities match", flush=True)


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/051-icon-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--icon-acceptance", "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            (DEST / (identity + ".log")).write_text(log, encoding="utf-8")
            expected = ("icon_acceptance=passed", "gpu_driver=direct3d12", "shader_format=DXIL",
                        "pointer_events=3 clicks=1 submits=36 content_runs=1", "catalog=848 layer_max=4 custom=3",
                        "rotation_cache=stable clip=passed motion=passed", "resize=1420x1000 idle_polls=3",
                        "deadline=none font_cleanup=passed disposed=1")
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
                extent = (1420, 1000) if name in RESIZED else (1600, 1100)
                with Image.open(target) as png:
                    if png.size != extent:
                        raise RuntimeError(identity + ": invalid GPU extent " + name)
                captures.append({"path": target.relative_to(DEST).as_posix(), "size": list(extent),
                                 "sha256": digest(target)})
            for left, right in (("initial", "rotate-45"), ("rotate-45", "rotate-90"),
                                ("rotate-90", "derived-colors"), ("derived-colors", "dark-primary"),
                                ("clip-on", "clip-off"), ("spin-first", "spin-second"),
                                ("spin-second", "reduced-motion"), ("source-four-layers", "hidden"),
                                ("resized", "pointer-custom"), ("popup-spin", "popup-closed")):
                if digest(output / (left + ".png")) == digest(output / (right + ".png")):
                    raise RuntimeError(identity + ": distinct states have identical readbacks")
            runs.append({"configuration": configuration, "mode": mode, "arguments": arguments,
                         "executable": executable.relative_to(ROOT).as_posix(), "executable_sha256": before,
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
