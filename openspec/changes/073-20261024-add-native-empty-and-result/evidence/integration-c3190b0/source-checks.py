import json
import re
import subprocess
from pathlib import Path

MAIN = "c3190b00210112a3a1ccf3a350cf0290082a4b14"
ORIGINAL = "a6017b1bcb10f38a551491302fc639652071afde"

def git_file(ref, path):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], text=True, encoding="utf-8")

def norm(text):
    return re.sub(r"\s+", "", text)

def block(text, start):
    opening = text.index("{", start)
    depth = 0
    for end in range(opening, len(text)):
        depth += (text[end] == "{") - (text[end] == "}")
        if depth == 0:
            return text[start:end + 1]
    raise AssertionError("unclosed source block")

snapshot_path = "src/theme/theme_snapshot.cpp"
main = git_file(MAIN, snapshot_path)
original = git_file(ORIGINAL, snapshot_path)
current = Path(snapshot_path).read_text(encoding="utf-8")
pattern = r"auto\s+(\w+)\s*=\s*\[&\]\s*\{"
main_lambdas = list(re.finditer(pattern, main[main.index("ThemeSnapshot resolve_theme("):]))
main_resolve = main[main.index("ThemeSnapshot resolve_theme("):]
current_resolve = current[current.index("ThemeSnapshot resolve_theme("):]
for match in main_lambdas:
    expected = block(main_resolve, match.start())
    found = re.search(r"auto\s+" + re.escape(match[1]) + r"\s*=\s*\[&\]\s*\{", current_resolve)
    assert found and norm(block(current_resolve, found.start())) == norm(expected), match[1]
assert len(main_lambdas) == 24, len(main_lambdas)
for name in ("empty", "result"):
    pattern = rf"\b{name}\s*=\s*\[&\]\s*\{{"
    expected = re.search(pattern, original)
    actual = re.search(pattern, current)
    assert expected and actual and norm(block(original, expected.start())) == norm(block(current, actual.start())), name

services = "src/component/window_component_services.cpp"
root_services = git_file(MAIN, services)
merged_services = Path(services).read_text(encoding="utf-8")
pattern = r"(\w+_)\s*=\s*std::make_unique<(\w+)>"
root_order = re.findall(pattern, root_services[:root_services.index("WindowComponentServices::~")])
merged_order = re.findall(pattern, merged_services[:merged_services.index("WindowComponentServices::~")])
assert [entry for entry in merged_order if entry[0] != "empty_result_"] == root_order
for host in ("Table", "Menu", "InputNumber", "Collapse", "Segmented", "EmptyResult"):
    if host == "InputNumber":
        input_source = Path("src/component/input_component.cpp").read_text(encoding="utf-8")
        assert "InputNumberState" in input_source and norm(input_source) == norm(git_file(MAIN, "src/component/input_component.cpp"))
    else:
        assert any(host + "ComponentHost" == entry[1] for entry in merged_order), host
for path in ("src/component/select_component.cpp", "src/component/dropdown_component.cpp"):
    assert norm(Path(path).read_text(encoding="utf-8")) == norm(git_file(MAIN, path)), path
for path in ("src/component/collapse_component.cpp", "src/component/collapse_component.hpp",
             "src/component/segmented_component.cpp", "src/component/segmented_component.hpp",
             "src/theme/segmented_theme.cpp", "src/theme/segmented_theme.hpp",
             "src/component/table_component.cpp", "src/component/table_component.hpp",
             "src/component/menu_component.cpp", "src/component/menu_component.hpp",
             "include/ryn/collapse.hpp", "include/ryn/segmented.hpp", "include/ryn/segmented_tokens.hpp"):
    assert norm(Path(path).read_text(encoding="utf-8")) == norm(git_file(MAIN, path)), path
for signature in ("CollapseThemeToken derive_collapse(", "void apply_collapse_override("):
    assert norm(block(current, current.index(signature))) == norm(block(main, main.index(signature))), signature
popconfirm_path = "src/component/popconfirm_component.cpp"
popconfirm_root = git_file(MAIN, popconfirm_path)
popconfirm_current = Path(popconfirm_path).read_text(encoding="utf-8")
signature = "void PopconfirmComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {"
expected_popconfirm = popconfirm_root.replace(signature, signature + "\n    if (mounted_.empty()) {\n        return;\n    }", 1)
assert norm(popconfirm_current) == norm(expected_popconfirm)
assert norm(block(merged_services, merged_services.index("bool WindowComponentServices::dispatch_scroll("))) == norm(block(root_services, root_services.index("bool WindowComponentServices::dispatch_scroll(")))
for path in ("include/ryn/empty.hpp", "include/ryn/result.hpp", "include/ryn/empty_result_tokens.hpp",
             "src/component/empty_result_component.hpp",
             "tests/empty_result_public_api_tests.cpp", "tests/empty_result_theme_tests.cpp"):
    assert norm(Path(path).read_text(encoding="utf-8")) == norm(git_file(ORIGINAL, path)), path
for path in ("src/component/empty_result_component.cpp", "tests/empty_result_component_tests.cpp"):
    assert norm(Path(path).read_text(encoding="utf-8")) == norm(git_file("28bee9e", path)), path
for path in subprocess.check_output(["git", "ls-tree", "-r", "--name-only", MAIN], text=True).splitlines():
    if path.startswith(("gallery/", "README", "docs/component-completion", "examples/gallery")):
        assert Path(path).read_bytes().replace(b"\r\n", b"\n") == subprocess.check_output(["git", "show", f"{MAIN}:{path}"]).replace(b"\r\n", b"\n"), path
tracked = set(subprocess.check_output(["git", "ls-files"], text=True).splitlines())
committed = set(subprocess.check_output(["git", "ls-tree", "-r", "--name-only", MAIN], text=True).splitlines())
committed |= set(subprocess.check_output(["git", "ls-tree", "-r", "--name-only", ORIGINAL], text=True).splitlines())
assert not committed - tracked
assert all(p.startswith("openspec/changes/073-20261024-add-native-empty-and-result/evidence/integration-c3190b0/")
           for p in tracked - committed), sorted(tracked - committed)
for path in tracked:
    if path.startswith("openspec/changes/074-"):
        assert Path(path).read_bytes().replace(b"\r\n", b"\n") == subprocess.check_output(["git", "show", f"{MAIN}:{path}"]).replace(b"\r\n", b"\n"), path
print(json.dumps({"main_resolution_lambdas_preserved": len(main_lambdas), "own_resolution_lambdas_preserved": 2,
                  "main_host_order_preserved": True, "main_wheel_routing_preserved": True,
                  "main_collapse_segmented_table_menu_input_number_preserved": True,
                  "popconfirm_guard_only": True,
                  "original_073_api_theme_preserved": True,
                  "review_r1_component_and_regression_preserved": "28bee9e",
                  "committed_global_docs_preserved": True,
                  "uncommitted_074_absent": True}, indent=2))
