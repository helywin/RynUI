"""Validate actual Windows SDL/D3D12 Tooltip windows and GPU readback hashes."""
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {"initial", "hover-edge", "escape", "disabled-child", "controlled", "focus",
         "dismissed", "dark", "compact", "no-arrow", "resized", "inactive"}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1.0"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2.0")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/040-tooltip-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--tooltip-acceptance", "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=60)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            (DEST / (identity + ".log")).write_text(log, encoding="utf-8")
            if result.returncode or "tooltip_acceptance=passed" not in log or before != digest(executable):
                raise RuntimeError(identity + " failed: " + log)
            bitmaps = {p.stem: p for p in raw.glob("*.bmp")}
            if set(bitmaps) != NAMES:
                raise RuntimeError(identity + " unexpected GPU readback inventory")
            captures = []
            for name in sorted(NAMES):
                target = output / (name + ".png")
                with Image.open(bitmaps[name]) as bitmap:
                    bitmap.convert("RGBA").save(target)
                with Image.open(target) as png:
                    captures.append({"path": target.relative_to(DEST).as_posix(), "size": list(png.size),
                                     "sha256": digest(target)})
            runs.append({"configuration": configuration, "mode": mode, "arguments": arguments,
                         "executable": executable.relative_to(ROOT).as_posix(), "executable_sha256": before,
                         "exit_code": result.returncode, "captures": captures})
            print(identity + ": passed; 12 GPU readbacks", flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for run in runs:
        assert digest(ROOT / run["executable"]) == run["executable_sha256"]
        for capture in run["captures"]:
            assert digest(DEST / capture["path"]) == capture["sha256"]
    print("Verified 10 actual runs, 120 PNG hashes and current Debug/Release executable hashes")

if __name__ == "__main__":
    main()
