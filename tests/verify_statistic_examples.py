"""Keep the documented Statistic example identical to the executed fixture."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
compiled = "".join((root / "tests/support/statistic_example.hpp").read_text(encoding="utf-8").split())
snippets = re.findall(r"```cpp\n(.*?)\n```", (root / "docs/statistic.md").read_text(encoding="utf-8"), re.S)
assert len(snippets) == 1
assert "".join(snippets[0].split()) in compiled, "Statistic documented example drifted from executed fixture"
print("Statistic documentation example matches the executed C++ fixture")
