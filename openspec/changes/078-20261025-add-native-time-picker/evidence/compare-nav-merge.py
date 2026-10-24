"""合并 Nav/Time 的源保留与 fresh golden typed leaf 比较。"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
MAIN = "c083fed5a72f644b491118c6440908e0354d241e"
TIME = "1116a9931d1c6ea07b25739319570bfd092f0bc3"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def old(ref, path):
    return git("show", f"{ref}:{path}").decode("utf-8").replace("\r\n", "\n")


def current(path):
    return (ROOT / path).read_text(encoding="utf-8")


def source_check():
    paths = git("ls-tree", "-r", "--name-only", MAIN).decode("utf-8").splitlines()
    main_prefixes = (
        "include/ryn/breadcrumb", "include/ryn/steps", "src/component/breadcrumb", "src/component/steps",
        "src/component/navigation_model", "src/theme/breadcrumb", "src/theme/steps", "tests/breadcrumb",
        "tests/steps", "tests/navigation", "tests/support/navigation", "tests/verify_navigation",
        "examples/token_gallery/", "gallery/", "docs/breadcrumb", "docs/steps", "docs/date-picker",
        "docs/form", "docs/component-completion", "README.md", "tests/token_gallery_frame_tests.cpp",
        "tests/button_component_tests.cpp", "tests/portable/CMakeLists.txt",
        "src/component/date_picker", "src/component/form_component", "include/ryn/date_picker",
        "include/ryn/form", "tests/date_picker", "tests/form_", "openspec/changes/079-", "openspec/changes/080-",
    )
    preserved = [p for p in paths if p.startswith(main_prefixes)]
    for path in preserved:
        assert current(path) == old(MAIN, path), ("main source changed", path)
    time_paths = git("ls-tree", "-r", "--name-only", TIME).decode("utf-8").splitlines()
    time_prefixes = (
        "include/ryn/time_", "src/component/time_", "src/theme/time_", "tests/time_", "docs/time-picker",
        "src/component/input_component.cpp", "src/component/input_component.hpp",
    )
    kept_time = [p for p in time_paths if p.startswith(time_prefixes)]
    for path in kept_time:
        assert current(path) == old(TIME, path), ("Time source changed", path)
    print(f"Git source preserved: main {len(preserved)} files, Time/Input {len(kept_time)} files")

    path = "src/theme/theme_snapshot.cpp"
    parent = old(MAIN, path)
    merged = current(path)
    start = "    auto button = [&] {"
    existing = parent[parent.index(start):parent.index("    return ThemeSnapshot(", parent.index(start))]
    retained = merged[merged.index(start):merged.index("    auto time_picker = [&] {", merged.index(start))]
    assert existing == retained, "Published main IIFEs changed"
    count = len(re.findall(r"= \[&\] \{", existing))
    assert count == 30, count
    time = old(TIME, path)
    start = "    auto time_picker = [&] {"
    expected = time[time.index(start):time.index("    return ThemeSnapshot(", time.index(start))]
    assert merged[merged.index(start):merged.index("    return ThemeSnapshot(", merged.index(start))] == expected
    print("main 30 IIFEs verbatim/in order; Time IIFE verbatim and appended")

    path = "src/theme/theme_runtime_types.hpp"
    parent = old(MAIN, path)
    start = "enum class TokenIdentity"
    prefix = parent[parent.index(start):parent.index("    count,", parent.index(start))]
    suffix = "    time_picker_colors,\n    time_picker_metrics,\n    time_picker_effects,\n    time_picker_order,\n"
    merged = current(path)
    assert merged[merged.index(start):].startswith(prefix + suffix + "    count,")
    print("Published TokenIdentity prefix unchanged; Time only follows Breadcrumb/Steps")

    path = "src/theme/theme_runtime.cpp"
    parent = old(MAIN, path)
    merged = current(path)
    start = "// Keep the large resolved snapshot temporary"
    end = "const StepsThemeToken& ThemeScope::steps_colors()"
    assert parent[parent.index(start):parent.index(end)] == merged[merged.index(start):merged.index(end)]
    header = current("include/ryn/theme.hpp")
    assert "resolve_theme(const ThemeConfig& config = detail::default_theme_config()," in header
    helper = "const ThemeConfig& detail::default_theme_config()"
    end = "ThemeSnapshot resolve_theme("
    assert time[time.index(helper):time.index(end, time.index(helper))] == current("src/theme/theme_snapshot.cpp")[
        current("src/theme/theme_snapshot.cpp").index(helper):current("src/theme/theme_snapshot.cpp").index(end)
    ]
    cpp = current("src/component/input_component.cpp")
    hpp = current("src/component/input_component.hpp")
    assert cpp.count("std::function<void()> on_owner_form_reset;") == 1
    assert cpp.count("void InputComponentHost::configure_form_reset(") == 1
    assert hpp.count("void configure_form_reset(") == 1
    assert cpp.count("bool InputComponentHost::focused(") == 1
    print("Static defaults, generic heap/config/bridge region and unique Input hook preserved")

    path = "src/component/window_component_services.cpp"
    merged = current(path)
    reduced = merged.replace('#include "component/time_picker_component.hpp"\n', '')
    reduced = reduced.replace('    time_picker_ = std::make_unique<TimePickerComponentHost>(*this);\n', '')
    reduced = reduced.replace('time_picker_->consume_scroll_boundary(event) || ', '')
    assert ''.join(reduced.split()) == ''.join(old(MAIN, path).split())
    hosts = re.findall(r'(\w+)_ = std::make_unique<(\w+)>', merged)
    assert hosts.index(('modal', 'ModalComponentHost')) < hosts.index(('table', 'TableComponentHost'))
    assert hosts.index(('date_picker', 'DatePickerComponentHost')) + 1 == hosts.index(
        ('time_picker', 'TimePickerComponentHost'))
    content_handler = '(*it)->on_scroll_input'
    boundary_fallback = 'return time_picker_->consume_scroll_boundary'
    assert merged.index(content_handler) < merged.index(boundary_fallback)
    print("All main host/cleanup/position/input logic preserved; Time added after Date, boundary content first")


def leaves(value, path=()):
    if isinstance(value, dict):
        return {p: v for key, child in value.items() for p, v in leaves(child, path + (key,)).items()}
    if isinstance(value, list):
        return {p: v for i, child in enumerate(value) for p, v in leaves(child, path + (i,)).items()}
    return {path: (type(value).__name__, value)}


def golden_check():
    for path in sorted((ROOT / "design-tokens/ant-design/6.6.5/golden").glob("*.json")):
        relative = path.relative_to(ROOT).as_posix()
        fresh = json.loads(path.read_text(encoding="utf-8"))
        for ref, added in [(MAIN, {"timePicker"}), (TIME, {"breadcrumb", "steps"})]:
            baseline = json.loads(old(ref, relative))
            assert set(fresh) - set(baseline) == added, (path.name, ref, "new fields")
            assert set(baseline) <= set(fresh)
            expected = leaves({k: v for k, v in baseline.items() if k != "identity"})
            actual = leaves({k: fresh[k] for k in baseline if k != "identity"})
            assert actual == expected, (path.name, ref, "typed values changed")
            assert json.dumps({k: fresh[k] for k in baseline if k != "identity"}, sort_keys=True) == json.dumps(
                {k: v for k, v in baseline.items() if k != "identity"}, sort_keys=True
            ), (path.name, ref, "container shape or typed value changed")
            print(f"{path.name} vs {ref[:7]}: {len(expected)} typed leaves preserved; added {sorted(added)}")
        print("sha256=" + hashlib.sha256(path.read_bytes()).hexdigest())


source_check()
if "--source-only" not in sys.argv:
    golden_check()
