"""Compare fresh Theme goldens and protected source to published and local baselines."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASES = {
    "62c990a1b494532c358ac487b2bb5931c08a7231": (28, {"breadcrumb", "steps", "timePicker", "descriptions"}),
    "c083fed5a72f644b491118c6440908e0354d241e": (30, {"timePicker", "descriptions"}),
    "580a8c3d235c22eb95b11688b509ee1e2ea65e44": (31, {"descriptions"}),
    "ee2b9efde55f32b1a111165c9d44e48c9286a387": (31, {"timePicker"}),
}


def old(base, path):
    return subprocess.check_output(["git", "show", f"{base}:{path}"], cwd=ROOT).decode("utf-8")


def compare(a, b, path):
    assert type(a) is type(b), f"type changed at {path}"
    if isinstance(a, dict):
        assert a.keys() == b.keys(), f"keys changed at {path}"
        return sum(compare(a[k], b[k], f"{path}.{k}") for k in a)
    if isinstance(a, list):
        assert len(a) == len(b), f"length changed at {path}"
        return sum(compare(x, y, f"{path}[{i}]") for i, (x, y) in enumerate(zip(a, b)))
    assert a == b, f"value changed at {path}: {a!r} != {b!r}"
    return 1


for base, (blocks, additions) in BASES.items():
    total = 0
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        before = json.loads(old(base, path))
        after = json.loads((ROOT / path).read_text(encoding="utf-8"))
        assert after.keys() - before.keys() == additions
        assert before["identity"] != after["identity"]
        del before["identity"]
        del after["identity"]
        for key in additions:
            del after[key]
        count = compare(before, after, name)
        total += count
        print(f"{base[:7]} {name}: {count} old typed values unchanged")

    path = "src/theme/theme_snapshot.cpp"
    pattern = r"^( +)(?:auto )?\w+ = \[&\] \{.*?^\1\}\(\);"
    before = [m.group() for m in re.finditer(pattern, old(base, path), re.M | re.S)]
    after = [m.group() for m in re.finditer(pattern, (ROOT / path).read_text(encoding="utf-8"), re.M | re.S)]
    assert len(before) == blocks, f"unexpected parent block count: {len(before)}"
    retained = after
    if base.startswith("ee2b9ef"):
        retained = [block for block in after if not block.lstrip().startswith("auto time_picker =")]
    assert len(after) == 32 and retained[:blocks] == before, "old resolver block text/order changed"
    print(f"PASS {base[:7]}: 5 fresh goldens; {total} old typed values; {blocks} old resolver blocks unchanged")

# The published main owns existing numeric identities. Descriptions is appended after TimePicker.
def identities(source):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\};", source, re.S).group(1)
    result = []
    value = 0
    for name, explicit in re.findall(r"^\s*(\w+)\s*(?:=\s*(\d+))?\s*,", body, re.M):
        if explicit:
            value = int(explicit)
        if name != "count":
            result.append((name, value))
        value += 1
    return result

path = "src/theme/theme_runtime_types.hpp"
before = identities(old("580a8c3d235c22eb95b11688b509ee1e2ea65e44", path))
after = identities((ROOT / path).read_text(encoding="utf-8"))
assert after[:len(before)] == before
assert [name for name, _ in after[len(before):]] == ["descriptions_colors", "descriptions_metrics"]
print(f"PASS: {len(before)} published TokenIdentity names/values/order retained; appended {after[len(before):]}")

published = "580a8c3d235c22eb95b11688b509ee1e2ea65e44"

def function_text(source, marker):
    assert source.count(marker) == 1, marker
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[start:position + 1]
    raise AssertionError(marker)

for path, markers in {
    "src/theme/theme_snapshot.cpp": ["const ThemeConfig& detail::default_theme_config()"],
    "src/theme/theme_runtime.cpp": [
        "std::shared_ptr<const ThemeSnapshot> resolve_validation_snapshot(",
        "std::shared_ptr<ThemeScope> ThemeScope::create_default()",
        "bool ThemeScope::update(const ThemeConfig& config)",
        "void ThemeScope::validate_children(const ThemeSnapshot& parent_snapshot) const",
        "void ThemeScope::commit_snapshot(const ThemeConfig& config, ThemeSnapshot&& next)",
    ],
}.items():
    before = old(published, path)
    after = (ROOT / path).read_text(encoding="utf-8")
    for marker in markers:
        assert function_text(before, marker) == function_text(after, marker), marker

protected = [
    "include/ryn/prop.hpp", "src/runtime/prop_connection.hpp", "src/component/theme_component.cpp",
    "src/component/time_picker_component.cpp", "src/component/time_picker_component.hpp",
    "src/component/time_picker_model.cpp", "src/component/time_picker_model.hpp",
    "src/theme/time_picker_theme.cpp", "src/theme/time_picker_theme.hpp",
    "include/ryn/time_picker.hpp", "include/ryn/time_picker_tokens.hpp",
    "src/component/breadcrumb_component.cpp", "src/component/breadcrumb_component.hpp",
    "src/component/steps_component.cpp", "src/component/steps_component.hpp",
]
for path in protected:
    assert old(published, path) == (ROOT / path).read_text(encoding="utf-8"), path
unchanged = subprocess.check_output([
    "git", "diff", "--name-only", published, "--", "examples", "gallery", "README.md", "README.en.md", "docs",
    "src/renderer", "src/platform",
    ":(exclude)docs/descriptions.md",
], cwd=ROOT).decode("utf-8")
assert not unchanged.strip(), unchanged
print("PASS: static defaults, heap validation/preflight/selfguard, Prop bridge, three hosts and existing Gallery/docs preserved")
