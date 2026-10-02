"""Check this change's Windows captures against its run manifest and binaries."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
root = parser.parse_args().root.resolve()
output = root / "openspec/changes/037-20261002-isolate-effect-packing-from-core/evidence/windows"
records = json.loads((output / "runs.json").read_text(encoding="utf-8"))
assert len(records) == 8, "Expected eight native acceptance runs"
seen = set()
captures = set()
for record in records:
    configuration, mode = record["configuration"], record["mode"]
    assert (configuration, mode) not in seen
    seen.add((configuration, mode))
    assert record["exit_code"] == 0
    executable = root / f"out/build/windows-msvc/examples/{configuration}/rynui_token_gallery.exe"
    assert hashlib.sha256(executable.read_bytes()).hexdigest() == record["executable_sha256"]
    for capture in record.get("captures", record.get("sizes", [])):
        relative = capture.get("path", capture.get("capture"))
        path = output / relative
        assert path.resolve().is_relative_to(output.resolve())
        assert relative not in captures
        captures.add(relative)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == capture["sha256"], relative
assert seen == {(configuration, mode) for configuration in ("Debug", "Release")
                for mode in ("system", "scale-2", "native-resize", "selection-scale-2")}
assert captures == {path.relative_to(output).as_posix() for path in output.rglob("*.png")}
assert len(captures) == 42, "Expected 42 PNG captures"
print(f"Verified {len(records)} runs, {len(captures)} PNG SHA256 values and current Debug/Release executable SHA256 values")
