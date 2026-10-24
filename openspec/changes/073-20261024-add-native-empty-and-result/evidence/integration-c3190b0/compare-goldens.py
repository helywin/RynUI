import json
import subprocess
from pathlib import Path

BASES = {
    "main-c3190b0": ("c3190b00210112a3a1ccf3a350cf0290082a4b14", {"empty", "result"}),
    "original-a6017b1": ("a6017b1bcb10f38a551491302fc639652071afde", {"collapse", "segmented"}),
}

def compare(old, new, path=""):
    if isinstance(old, dict):
        assert isinstance(new, dict), path
        if path:
            assert set(old) == set(new), path
        return sum(compare(v, new[k], path + "/" + k) for k, v in old.items() if not (not path and k == "identity"))
    if isinstance(old, list):
        assert isinstance(new, list) and len(old) == len(new), path
        return sum(compare(a, b, path + "/" + str(i)) for i, (a, b) in enumerate(zip(old, new)))
    assert old == new, (path, old, new)
    return 1

results = {}
for name, (ref, additions) in BASES.items():
    files = {}
    for p in sorted(Path("design-tokens/ant-design/6.6.5/golden").glob("*.json")):
        old = json.loads(subprocess.check_output(["git", "show", f"{ref}:{p.as_posix()}"]))
        new = json.loads(p.read_text(encoding="utf-8"))
        assert set(new) - set(old) == additions, (name, p.name, set(new) - set(old))
        files[p.name] = compare(old, new)
    assert len(files) == 5
    results[name] = {"preserved_leaf_values": files, "total": sum(files.values()),
                     "new_component_fields": sorted(additions), "ignored_fields": ["identity"]}
print(json.dumps(results, indent=2))
