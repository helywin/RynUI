"""Convert the locked Unicode conformance input to a compact offline fixture."""
import argparse
import hashlib
from pathlib import Path
import re


def render(source: bytes, expected_hash: str) -> bytes:
    if hashlib.sha256(source).hexdigest() != expected_hash:
        raise ValueError("GraphemeBreakTest source SHA256 mismatch")
    lines = ["# Unicode GraphemeBreakTest 17.0.0; derived data; Unicode-3.0",
             "# Copyright Unicode, Inc.; https://www.unicode.org/license.txt",
             "# source-sha256 " + expected_hash]
    for line in source.decode("utf-8").splitlines():
        tokens = line.split("#", 1)[0].split()
        if not tokens:
            continue
        if tokens[0] != "÷" or tokens[-1] != "÷" or len(tokens) % 2 != 1:
            raise ValueError("Malformed boundary row")
        text = bytearray()
        boundaries = []
        for index, token in enumerate(tokens):
            if index % 2 == 0:
                if token == "÷":
                    boundaries.append(len(text))
                elif token != "×":
                    raise ValueError("Malformed boundary marker")
            else:
                text.extend(chr(int(token, 16)).encode("utf-8"))
        lines.append(text.hex() + " " + ",".join(map(str, boundaries)))
    if len(lines) <= 3:
        raise ValueError("Empty corpus")
    return ("\n".join(lines) + "\n").encode("ascii")


def check(actual: bytes, expected: bytes) -> None:
    if actual != expected:
        raise ValueError("Stale generated grapheme fixture")


def main() -> None:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    match = re.search(r'set\(RYNUI_GRAPHEME_TEST_SOURCE_SHA256\s+"([0-9a-f]{64})"\)',
                      args.lock.read_text(encoding="utf-8"))
    if match is None:
        raise ValueError("Missing corpus identity")
    source = args.source.read_bytes()
    expected = render(source, match[1])
    if args.check:
        check(args.output.read_bytes(), expected)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(expected)
    if args.self_test:
        for operation in (lambda: render(source + b"# stale", match[1]),
                          lambda: check(expected + b"stale", expected),
                          lambda: check(expected[:-1], expected)):
            try:
                operation()
            except ValueError:
                continue
            raise AssertionError("Corrupt input/output was not rejected")
    print(f"Unicode 17 corpus: {len(expected.splitlines()) - 3} cases; SHA and output verified")


if __name__ == "__main__":
    main()
