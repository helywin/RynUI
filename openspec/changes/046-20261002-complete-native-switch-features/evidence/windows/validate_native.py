"""Validate Windows Switch windows, executable identities and actual GPU readbacks."""
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
NAMES = {"initial", "keyboard-pressed", "keyboard-wave", "keyboard-settled", "hover", "pointer-pressed",
         "wave-start", "wave-middle", "wave-finished", "loading-cancel", "disabled-cancel", "controlled-no-echo",
         "rtl-small-icon", "narrow-40", "narrow-10", "narrow-recovered", "dark", "compact", "resized", "inactive"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    runs = []
    for configuration in ("Debug", "Release"):
        executable = ROOT / "out/build/windows-msvc/examples" / configuration / "rynui_token_gallery.exe"
        for mode, scale in (("system", None), ("scale-1", "1"), ("scale-1.25", "1.25"),
                            ("scale-1.5", "1.5"), ("scale-2", "2")):
            identity = configuration.lower() + "-" + mode
            raw = ROOT / "out/046-switch-windows" / identity
            raw.mkdir(parents=True, exist_ok=True)
            output = DEST / identity
            output.mkdir(parents=True, exist_ok=True)
            arguments = [str(executable), "--switch-acceptance", "--evidence-dir=" + str(raw)]
            if scale is not None:
                arguments.append("--acceptance-scale=" + scale)
            before = digest(executable)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            (DEST / (identity + ".log")).write_text(log, encoding="utf-8")
            expected = ("switch_acceptance=passed", "gpu_driver=direct3d12", "shader_format=DXIL", "normalized_events=11",
                        "changes=2 clicks=2 candidates=1", "submits=40", "content_runs=1 slot_runs=12", "deadline=none disposed=1")
            if result.returncode or any(value not in log for value in expected) or before != digest(executable):
                raise RuntimeError(identity + " failed: " + log)
            bitmaps = {path.stem: path for path in raw.glob("*.bmp")}
            if set(bitmaps) != NAMES:
                raise RuntimeError(identity + " unexpected GPU readback inventory")
            captures = []
            for name in sorted(NAMES):
                target = output / (name + ".png")
                with Image.open(bitmaps[name]) as bitmap:
                    bitmap.convert("RGBA").save(target)
                with Image.open(target) as png:
                    expected_size = (1420, 900) if name in {"resized", "inactive"} else (1600, 1000)
                    if png.size != expected_size:
                        raise RuntimeError(identity + ": invalid GPU extent " + name)
                    captures.append({"path": target.relative_to(DEST).as_posix(), "size": list(png.size),
                                     "sha256": digest(target)})
            for left, right in (("wave-start", "wave-middle"), ("narrow-40", "narrow-10"), ("initial", "dark")):
                if digest(output / (left + ".png")) == digest(output / (right + ".png")):
                    raise RuntimeError(identity + ": distinct visual states produced the same readback")
            runs.append({"configuration": configuration, "mode": mode, "arguments": arguments,
                         "executable": executable.relative_to(ROOT).as_posix(), "executable_sha256": before,
                         "exit_code": result.returncode, "captures": captures})
            print(identity + f": passed; {len(NAMES)} GPU readbacks", flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for run in runs:
        assert digest(ROOT / run["executable"]) == run["executable_sha256"]
        for capture in run["captures"]:
            assert digest(DEST / capture["path"]) == capture["sha256"]
    print(f"Verified 10 actual runs, {len(NAMES) * 10} PNG hashes and current executable hashes")


if __name__ == "__main__":
    main()
