"""Compare fixed GPU readback fixtures with the accepted pre-migration renderer."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
root = parser.parse_args().root.resolve()
baseline = root / "openspec/changes/037-20261002-isolate-effect-packing-from-core/evidence/windows"
current = root / "openspec/changes/038-20261002-complete-renderer-backend-contracts/evidence/windows"
compared = 0
for configuration in ("debug", "release"):
    for mode in ("system", "scale-2"):
        images = sorted((current / f"{configuration}-{mode}").glob("*.png"))
        assert len(images) == 9
        for path in images:
            old = baseline / path.relative_to(current)
            with Image.open(old) as before, Image.open(path) as after:
                assert before.size == after.size
                assert before.convert("RGBA").tobytes() == after.convert("RGBA").tobytes(), str(path)
            compared += 1
print(f"Compared {compared} fixed Typography GPU readbacks with change 037: identical pixel dimensions and RGBA bytes")
