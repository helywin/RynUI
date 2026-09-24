"""Offline contract for the pinned Switch and Checkbox source evidence."""

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
CHANGE = ROOT / "openspec/changes/011-20260922-build-switch-and-checkbox-on-shared-control-runtime"


def verify() -> None:
    contract = json.loads((CHANGE / "evidence/source-contract.json").read_text(encoding="utf-8"))
    manifest = json.loads((ROOT / "gallery/ant-design/6.6.5/source-manifest.json").read_text(encoding="utf-8"))
    diff = json.loads((ROOT / "openspec/changes/012-20260922-upgrade-ant-design-reference-to-6-6-5/evidence/source-diff.json").read_text(encoding="utf-8"))
    assert contract["upstream_version"] == manifest["upstream"]["version"] == diff["upstream"]["new_version"]
    assert contract["upstream_commit"] == manifest["upstream"]["commit"] == diff["upstream"]["new_commit"]
    sources = {entry["path"]: entry for entry in diff["sources"]}
    for path, sha in contract["sources"].items():
        assert sources[path]["sha256_665"] == sha, path
        local_source = ROOT / "out/upstream-ant-design-6.6.5" / path
        if local_source.is_file():
            assert hashlib.sha256(local_source.read_bytes()).hexdigest() == sha, path
    components = {entry["identity"]: entry for entry in diff["tokens"]}
    for name in contract["switch"]["component_tokens"]:
        assert f"ant.component.Switch.{name}" in components, name
    assert not any(identity.startswith("ant.component.Checkbox.") for identity in components)
    switch_doc = (ROOT / "out/upstream-ant-design-6.6.5/components/switch/index.en-US.md")
    checkbox_doc = (ROOT / "out/upstream-ant-design-6.6.5/components/checkbox/index.en-US.md")
    if switch_doc.is_file() and checkbox_doc.is_file():
        switch_text = switch_doc.read_text(encoding="utf-8")
        checkbox_text = checkbox_doc.read_text(encoding="utf-8")
        for name in contract["switch"]["props"]:
            assert f"| {name} |" in switch_text, name
        for name in contract["checkbox"]["props"]:
            assert f"| {name} |" in checkbox_text, name
        assert "`medium` `small`" in switch_text
        assert "| size |" not in checkbox_text
    assert contract["switch"]["sizes"] == ["medium", "small"]
    assert "loading" in contract["switch"]["states"]
    assert "indeterminate" in contract["checkbox"]["states"]
    assert contract["checkbox"]["size_source"] == "controlInteractiveSize"
    assert contract["checkbox"]["indeterminate_indicator_source"] == "fontSizeLG / 2"


if __name__ == "__main__":
    try:
        verify()
    except (AssertionError, KeyError) as exc:
        sys.exit(f"selection source contract failed: {exc}")
    print("selection source contract passed")
