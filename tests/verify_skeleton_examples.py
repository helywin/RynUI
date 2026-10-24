"""Keep the documented Skeleton example identical to its executed fixture."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
compiled = "".join((root / "tests/skeleton_integration_tests.cpp").read_text(encoding="utf-8").split())
snippets = re.findall(r"```cpp\n(.*?)\n```", (root / "docs/skeleton.md").read_text(encoding="utf-8"), re.S)
assert len(snippets) == 1
assert "".join(snippets[0].split()) in compiled, "Skeleton documented example drifted from compiled fixture"
print("Skeleton documentation example matches the executed C++ fixture")
