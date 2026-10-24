"""Extract the actual public C++ examples for the MSVC compile check."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[4]
examples = re.findall(r"```cpp\n(.*?)\n```", (root / "docs/descriptions.md").read_text(encoding="utf-8"), re.S)
assert len(examples) == 2
target = root / "out/descriptions-doc-examples.cpp"
target.write_text("\n\n".join(examples) + "\n", encoding="utf-8")
print(f"Extracted {len(examples)} actual Descriptions documentation examples")
