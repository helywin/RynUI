"""Import/check all SVGs from the dependency-locked icons archive.

Does not download or extract archive paths. Each accepted member is written
to an explicitly constructed filename inside third_party/ant-design-icons.
"""
import argparse
import hashlib
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / "third_party/ant-design-icons"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=ROOT / "out/icons-svg-4.6.0.tgz")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    lock = (ROOT / "cmake/dependencies/RynUIDependencyLock.cmake").read_text(encoding="utf-8")
    expected = re.search(r'RYNUI_ANT_ICONS_SOURCE_SHA256\s+"([0-9a-f]{64})"', lock).group(1)
    if hashlib.sha256(args.archive.read_bytes()).hexdigest() != expected:
        raise SystemExit("Icon archive does not match the dependency lock")
    sources = {}
    with tarfile.open(args.archive) as archive:
        for member in archive.getmembers():
            parts = member.name.split("/")
            if len(parts) != 4 or parts[:2] != ["package", "inline-svg"]:
                continue
            if parts[2] not in ("outlined", "filled", "twotone") or not parts[3].endswith(".svg"):
                continue
            if not member.isfile() or not re.fullmatch(r"[a-z0-9-]+\.svg", parts[3]):
                raise SystemExit("Unexpected locked icon archive member")
            name = parts[2] + "-" + parts[3]
            if name in sources:
                raise SystemExit("Duplicate archive member")
            sources[name] = archive.extractfile(member).read()
    if len(sources) != 848:
        raise SystemExit("Locked archive must contain 848 SVGs")
    for name, data in sources.items():
        path = TARGET / name
        if args.check:
            if path.read_bytes() != data:
                raise SystemExit("Icon source differs from archive: " + name)
        else:
            path.write_bytes(data)
    if {path.name for path in TARGET.glob("*.svg")} != set(sources):
        raise SystemExit("Committed SVG inventory differs from the locked archive")
    print("848 SVGs match the locked source archive" if args.check else "Imported 848 locked SVGs")


if __name__ == "__main__":
    main()
