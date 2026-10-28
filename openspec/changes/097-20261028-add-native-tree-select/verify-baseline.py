"""Verify the TreeSelect addition against its published API/model baseline."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASE = "d438e070c2815c5560dde836adab58b1b0d591ee"


def blob(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT)


def recipes(source):
    return [m.group(0) for m in re.finditer(r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", source, re.M | re.S)]


def identities(source):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [p.strip() for p in re.sub(r"//[^\n]*", "", body).split(",") if p.strip() and p.strip() != "count"]


def compare(old, new, path=""):
    assert type(old) is type(new), f"type changed: {path}"
    if isinstance(old, dict):
        assert old.keys() <= new.keys(), f"keys removed: {path}"
        if path:
            assert old.keys() == new.keys(), f"old nested object changed shape: {path}"
        else:
            assert new.keys() - old.keys() == {"treeSelect"}, "unexpected new component field"
        return sum(compare(value, new[key], f"{path}/{key}") for key, value in old.items() if path or key != "identity")
    if isinstance(old, list):
        assert len(old) == len(new), f"array changed length: {path}"
        return sum(compare(a, b, f"{path}/{i}") for i, (a, b) in enumerate(zip(old, new)))
    assert old == new, f"value changed: {path}"
    if isinstance(old, float) and old == 0:
        assert math.copysign(1, old) == math.copysign(1, new), f"signed zero changed: {path}"
    return 1


def before_manual_request_fix(path, source):
    if path == "src/component/popover_component.cpp":
        addition = '''

    struct ManualRequestGuard final {
        WindowComponentServices& services;
        runtime::ComponentId id;

        ~ManualRequestGuard() noexcept {
            auto* live = services.components().state<PopoverState>(id);
            if (live && !live->triggers.hover && !live->triggers.focus && !live->triggers.click) {
                live->model.settle_request();
            }
        }
    } guard{*services_, id};

'''
    elif path == "src/component/popover_model.hpp":
        addition = '''

    // A manual controller may reject an intent without changing its open prop.
    // Finish that delivery so the next explicit dismissal can request again.
    void settle_request() noexcept {
        if (controlled_) {
            requested_ = open_;
        }
    }

'''
    else:
        return source
    assert source.count(addition) == 1, f"declared Manual request addition drifted: {path}"
    return source.replace(addition, "\n", 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fresh", type=Path)
    parser.add_argument("--write-goldens", action="store_true")
    args = parser.parse_args()
    for path, parse, additions in [("src/theme/theme_snapshot.cpp", recipes, 1),
                                    ("src/theme/theme_runtime_types.hpp", identities, 6)]:
        old = parse(blob(path).decode("utf-8").replace("\r\n", "\n"))
        new = parse((ROOT / path).read_text(encoding="utf-8"))
        assert new[:len(old)] == old and len(new) == len(old) + additions, path
        print(f"{path}: {len(old)} old entries preserved, {additions} appended")
    protected = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", BASE,
        "src/renderer", "src/platform", "cmake/RynUIShaders.cmake", "cmake/EmbedBuiltinShaders.cmake",
        "src/input", "src/component/tree_component.cpp", "src/component/auto_complete_component.cpp",
        "src/component/input_component.cpp", "src/component/popover_component.cpp", "src/component/popover_model.hpp",
        "src/component/date_picker_component.cpp", "src/component/time_picker_component.cpp",
        "src/component/statistic_component.cpp", "src/component/skeleton_component.cpp", "examples", "gallery"
        ], cwd=ROOT, text=True).splitlines()
    for path in protected:
        source = (ROOT / path).read_text(encoding="utf-8")
        assert before_manual_request_fix(path, source) == blob(path).decode("utf-8").replace("\r\n", "\n"), path
    print(f"Protected resource, host, input and Gallery files: {len(protected)-2} unchanged; 2 exact Manual request additions")
    if args.fresh:
        total = 0
        for name in ["default", "dark", "compact", "dark-compact", "compact-dark"]:
            path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
            fresh = (args.fresh / f"{name}.json").read_bytes()
            total += compare(json.loads(blob(path)), json.loads(fresh))
            if args.write_goldens:
                (ROOT / path).write_bytes(fresh)
            print(f"{name}: old typed values preserved; SHA256 {hashlib.sha256(fresh).hexdigest()}")
        print(f"Old typed leaves preserved: {total}")


if __name__ == "__main__":
    main()
