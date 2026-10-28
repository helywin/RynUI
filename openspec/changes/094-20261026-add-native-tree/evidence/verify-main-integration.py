"""Check the reviewed Tree/AutoComplete merge against the published main baseline."""
from pathlib import Path
import argparse
import json
import math
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
MAIN = "48bb20f8bfb9663cc48adeaae58cbc946aec7af1"
WORKER = "972ca5bcaaa34ac90207da7e4703dd0ec032d360"


def blob(ref, path):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT)


def text(ref, path):
    return blob(ref, path).decode("utf-8").replace("\r\n", "\n")


def recipes(source):
    return [m.group(0) for m in re.finditer(r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", source, re.M | re.S)]


def identities(source):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [p.strip() for p in re.sub(r"//[^\n]*", "", body).split(",") if p.strip() and p.strip() != "count"]


def typed_equal(a, b):
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(typed_equal(value, b[key]) for key, value in a.items())
    if isinstance(a, list):
        return len(a) == len(b) and all(typed_equal(x, y) for x, y in zip(a, b))
    if isinstance(a, float) and a == 0 and b == 0:
        return math.copysign(1, a) == math.copysign(1, b)
    return a == b


def leaves(value):
    if isinstance(value, dict):
        return sum(leaves(v) for v in value.values())
    if isinstance(value, list):
        return sum(leaves(v) for v in value)
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-only", action="store_true")
    parser.add_argument("--write-goldens", action="store_true")
    parser.add_argument("--config", choices=["Debug", "Release"], default="Debug")
    args = parser.parse_args()
    for path, parse, label in [("src/theme/theme_snapshot.cpp", recipes, "IIFEs"),
                               ("src/theme/theme_runtime_types.hpp", identities, "TokenIdentity")]:
        a, b = [parse(text(ref, path)) for ref in [MAIN, WORKER]]
        expected = a + [item for item in b if item not in a]
        actual = parse((ROOT / path).read_text(encoding="utf-8"))
        assert actual == expected, f"{label}: old main prefix or reviewed worker body changed"
        assert len(actual) == len(set(actual)), f"{label}: duplicate entry"
        print(f"{label}: main prefix {len(a)} + worker additions {len(expected)-len(a)} = {len(actual)}")

    frozen_main = ["CMakeLists.txt", "cmake/RynUIArchitecture.cmake", "cmake/RynUIShaders.cmake",
                   "cmake/EmbedBuiltinShaders.cmake", "examples/CMakeLists.txt",
                   "examples/token_gallery/token_gallery_definition.cpp",
                   "examples/token_gallery/token_gallery_runtime.cpp", "src/component/skeleton_component.cpp",
                   "include/ryn/statistic.hpp"]
    frozen_main += subprocess.check_output(["git", "ls-tree", "-r", "--name-only", MAIN,
                                           "src/renderer/sdl", "src/component/rate_component.cpp",
                                           "src/component/rate_component.hpp"], cwd=ROOT, text=True).splitlines()
    for path in frozen_main:
        assert (ROOT / path).read_bytes().replace(b"\r\n", b"\n") == blob(MAIN, path).replace(b"\r\n", b"\n"), path
    frozen_worker = ["src/input/focus_manager.cpp", "src/input/focus_manager.hpp", "src/input/pointer_router.cpp",
                     "src/input/pointer_router.hpp", "src/component/auto_complete_component.cpp",
                     "src/component/tree_component.cpp", "tests/component_failure_retirement_tests.cpp"]
    for path in frozen_worker:
        assert (ROOT / path).read_bytes().replace(b"\r\n", b"\n") == blob(WORKER, path).replace(b"\r\n", b"\n"), path
    print(f"Frozen main resource/component files {len(frozen_main)} and reviewed shared/host files {len(frozen_worker)} retained")
    # The only compatibility fix reuses quiet retirement at the two failed-mount boundaries.
    def statistic_outside_failure_boundaries(source):
        for expression in [r"^        ~MountRollback\(\) noexcept \{.*?^        \}\n",
                           r"^                        c\.mount_slot_with_semantic_text_style\(\n.*?"
                           r"^                            Prop<runtime::SemanticTypography>.*?\);\n"]:
            source, count = re.subn(expression, "<verified-failure-boundary>\n", source, flags=re.M | re.S)
            assert count == 1, "Statistic failure boundary moved unexpectedly"
        return source
    path = "src/component/statistic_component.cpp"
    assert statistic_outside_failure_boundaries(text(MAIN, path)) == statistic_outside_failure_boundaries(
        (ROOT / path).read_text(encoding="utf-8")), "Statistic changed outside its two failure boundaries"
    print("Statistic retained outside the two regression-covered quiet-retirement boundaries")
    if args.source_only:
        return

    pending = []
    counts = {MAIN: 0, WORKER: 0}
    for name in ["default", "dark", "compact", "dark-compact", "compact-dark"]:
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        folder = "094-main-" + args.config.lower()
        fresh = (ROOT / "out" / (folder + "-tree-goldens") / (name + ".json")).read_bytes()
        other = (ROOT / "out" / (folder + "-ac-goldens") / (name + ".json")).read_bytes()
        assert fresh == other, f"{name}: production exporters disagree"
        actual = json.loads(fresh)
        parents = [json.loads(blob(ref, path)) for ref in [MAIN, WORKER]]
        assert actual.keys() == parents[0].keys() | parents[1].keys(), f"{name}: unexpected root fields"
        for ref, old in zip([MAIN, WORKER], parents):
            for key, value in old.items():
                if key != "identity":
                    assert typed_equal(value, actual[key]), f"{name}/{ref}/{key}: typed value changed"
                    counts[ref] += leaves(value)
        if not args.write_goldens:
            assert (ROOT / path).read_bytes() == fresh, f"{name}: tracked golden differs"
        pending.append((ROOT / path, fresh))
    if args.write_goldens:
        for path, fresh in pending:
            path.write_bytes(fresh)
    print(f"Five {args.config} fresh goldens: both exporters match; old typed leaves main={counts[MAIN]}, worker={counts[WORKER]}")


if __name__ == "__main__":
    main()
