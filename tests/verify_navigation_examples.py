"""Keep the compiled examples identical to both public documentation snippets."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
compiled = "".join((root / "tests/navigation_examples_tests.cpp").read_text(encoding="utf-8").split())
for name in ("breadcrumb", "steps"):
    source = (root / f"docs/{name}.md").read_text(encoding="utf-8")
    snippets = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
    assert len(snippets) == 1, name
    assert "".join(snippets[0].split()) in compiled, f"{name} documentation drifted from compiled example"
print("Both documented C++ snippets match the compiled examples")
