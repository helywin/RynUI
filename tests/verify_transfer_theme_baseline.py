"""Verify all pre-Transfer Theme fields and the complete new typed golden shape."""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "2777cdc3e7076247aadeac66f28088fbaeb4eb49"


def old(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode()


def main():
    path = "src/theme/theme_runtime_types.hpp"
    def identities(source):
        body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S)[1]
        return [p.strip() for p in re.sub(r"//[^\n]*", "", body).split(",") if p.strip()]
    before = identities(old(path))
    after = identities((ROOT / path).read_text())
    assert after[:len(before)-1] == before[:-1], "frozen token identity order changed"
    added = [f"transfer_{x}" for x in ("colors", "metrics", "typography", "effects", "order")]
    assert [name for name in after if name.startswith("transfer_")] == added
    assert after[-1] == "count"
    path = "src/theme/theme_runtime.cpp"
    def names(source):
        body = source.split("std::string_view token_identity_name(", 1)[1]
        body = body.split("constexpr std::array names{", 1)[1].split("};", 1)[0]
        return re.findall(r'"([^"\n]+)"', body)
    old_names = names(old(path))
    new_names = names((ROOT / path).read_text())
    assert new_names == old_names + [f"Transfer.{x}" for x in ("colors", "metrics", "typography", "effects", "order")]
    assert len(new_names) == len(after) - 1, "token identity names/count drifted"
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        expected = json.loads(old(path))
        actual = json.loads((ROOT / path).read_text())
        assert len(expected) == 60, "base no longer has 60 frozen fields"
        assert actual.pop("identity") != expected.pop("identity")
        transfer = actual.pop("transfer")
        assert expected.keys() == actual.keys(), f"frozen Theme fields removed: {name}"
        assert all(actual[key] == value for key, value in expected.items()), f"old full Theme fields changed: {name}"
        assert set(transfer) == {"colors", "metrics", "typography", "fontFamily", "effects", "order"}
        assert len(transfer["colors"]) == 17 and len(transfer["metrics"]) == 13
        assert len(transfer["typography"]) == 2 and len(transfer["effects"]) == 2
        assert all(len(c) == 4 for c in transfer["colors"])
        assert transfer["order"] == 0 and transfer["fontFamily"] == 0
        expected_metrics = [35, 28, 4, 180, 200, 250, 35, 8, 28, 4, 35, 6, 1] if "compact" in name else [40, 32, 5, 180, 200, 250, 40, 12, 32, 8, 40, 6, 1]
        assert transfer["metrics"] == expected_metrics, f"complete Transfer metric shape drifted: {name}"
        assert transfer["typography"] == ([12, 20] if "compact" in name else [14, 22])
        assert transfer["typography"][0] > 0 and transfer["effects"] == [3, 1]
    print("Transfer golden baseline: 5 complete shapes; all 60 prior top-level fields preserved except identity")


if __name__ == "__main__":
    main()
