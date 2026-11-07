#!/usr/bin/env python3
"""Check authored Gallery message schemas, source identities and registered literals without compiling."""
import argparse
from functools import lru_cache
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "examples/token_gallery"
STRING = re.compile(r'(?:u8)?"(?:[^"\\]|\\.)*"')


def strings(text):
    result = []
    end = None
    for match in STRING.finditer(text):
        value = json.loads(match[0].removeprefix("u8"))
        if end is not None and not text[end:match.start()].strip():
            result[-1] += value
        else:
            result.append(value)
        end = match.end()
    return result


def closing(text, start):
    depth = 1
    position = start + 1
    while position < len(text):
        match = STRING.match(text, position)
        if match:
            position = match.end()
            continue
        if text[position] == "{":
            depth += 1
        elif text[position] == "}":
            depth -= 1
            if not depth:
                return position
        position += 1
    raise ValueError("Unclosed authored message declaration")


def parameters(pattern):
    text = pattern.replace("{{", "").replace("}}", "")
    values = re.findall(r"\{([a-zA-Z_][a-zA-Z_0-9]*)\}", text)
    residue = re.sub(r"\{[a-zA-Z_][a-zA-Z_0-9]*\}", "", text)
    if "{" in residue or "}" in residue:
        raise ValueError("Malformed authored Gallery message pattern")
    return set(values)


@lru_cache(maxsize=12)
def source_literals(text):
    """Yield decoded literals and spans, ignoring comments and includes."""
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|(?:u8)?"(?:[^"\\]|\\.)*"')
    result = []
    for match in token.finditer(text):
        if match[0].startswith(("//", "/*")):
            continue
        if text[text.rfind("\n", 0, match.start()) + 1:match.start()].lstrip().startswith("#"):
            continue
        value = json.loads(match[0].removeprefix("u8"))
        if result and not text[result[-1][2]:match.start()].strip():
            previous = result.pop()
            result.append((previous[0] + value, previous[1], match.end()))
        else:
            result.append((value, match.start(), match.end()))
    return result


@lru_cache(maxsize=12)
def function_headers(text):
    return [(match.start(), match[1]) for match in re.finditer(
        r"(?m)^(?:[\w:<>*&]+(?: [\w:<>*&]+)*\s+)?([\w:]+)\([^;\n]*\)\s*(?:const\s*)?\{", text)]


def containing_function(text, position):
    state = text.rfind("struct GalleryState final", 0, position)
    if state >= 0 and "void visit_gallery_draft_fields" not in text[state:position]:
        return "GalleryState"
    functions = [name for start, name in function_headers(text) if start <= position]
    return functions[-1] if functions else "file"


def unlocalized_ui(path, text, fixtures):
    """Inspect actual authored display sinks rather than the script of their text.

    Fixtures require the exact source, function and literal. Identifiers do not
    exempt a literal passed to a display sink. Computed strings with a literal
    prefix are reported too, so event feedback cannot bypass this check.
    """
    failures = []
    sink = re.compile(r"(?:\b(?:Text|Title|Paragraph|Link|document_text|paragraph)\s*\(|\.(?:content|title|subtitle|description|placeholder|label|text|help|extra|tooltip|set|required)\s*\()\s*(?:(?:ryn::)?(?:String|utf8)\s*(?:\{|\())?\s*$")
    model_label = re.compile(
        r"\b(?:TabItem|MenuItem|TreeItem|TreeSelectItem|BreadcrumbItem|TimelineItem|StepsItem|AnchorItem)"
        r"\s*\{\s*(?:(?:u8)?\"(?:[^\"\\]|\\.)*\"|\w+)\s*,\s*"
        r"(?:(?:ryn::)?String\s*\{)?\s*$")
    for value, start, end in source_literals(text):
        before = text[max(0, start - 180):start]
        if not sink.search(before) and not model_label.search(before):
            continue
        function = containing_function(text, start)
        if not value:
            continue
        if (path, function, value) in fixtures:
            continue
        # Only punctuation, numbers and physical/API notation are language-neutral.
        if re.fullmatch(r"[\d\s.,:%+−…›‹/()×–—-]+", value):
            continue
        if value.startswith("gallery."):
            failures.append((start, "automation identity passed to visible text", value))
        else:
            failures.append((start, "unbound application text", value))
    return [f"{path}:{text.count(chr(10), 0, start) + 1}: {reason}: {value!r}" for start, reason, value in failures]


def check(root=ROOT):
    global DIRECTORY
    DIRECTORY = root / "examples/token_gallery"
    source = (DIRECTORY / "gallery_i18n.cpp").read_text(encoding="utf-8")
    declarations = {}
    for match in re.finditer(r"MessageKey\{", source):
        end = closing(source, match.end() - 1)
        declaration = source[match.end():end]
        literals = strings(declaration)
        identity, english = literals[:2]
        tail = source[end + 1:]
        chinese_match = re.match(r",\s*String\{", tail)
        if not chinese_match:
            raise ValueError(f"No explicit Chinese message: {identity}")
        chinese_end = closing(tail, chinese_match.end() - 1)
        chinese = "".join(strings(tail[chinese_match.end():chinese_end]))
        if re.search(r"\?{3,}", chinese):
            raise ValueError(f"Corrupted Chinese message: {identity}")
        schema = set(re.findall(r'\{u8"([^"\n]+)",\s*MessageArgType::\w+\}', declaration))
        if identity in declarations or not identity.startswith("gallery."):
            raise ValueError(f"Duplicate/invalid message identity: {identity}")
        if parameters(english) != schema or parameters(chinese) != schema:
            raise ValueError(f"Message parameters differ between languages: {identity}")
        if "gallery." in english or "gallery." in chinese:
            raise ValueError(f"Automation key in visible message: {identity}")
        declarations[identity] = (english, chinese)
    if not declarations:
        raise ValueError("No authored Gallery messages found")
    for path in DIRECTORY.glob("*.cpp"):
        if path.name.startswith("generated_") or path.name == "gallery_i18n.cpp":
            continue
        text = path.read_text(encoding="utf-8")
        for identity in re.findall(r'gallery_(?:label|text|message)\("(gallery\.[^"]+)"', text):
            if identity not in declarations:
                raise ValueError(f"Undeclared message in {path.name}: {identity}")
    definition = (DIRECTORY / "token_gallery_definition.cpp").read_text(encoding="utf-8")
    samples = set(re.findall(r'\{\{"(gallery\.sample\.[^"]+)"', definition))
    for identity in samples:
        if identity + ".title" not in declarations:
            raise ValueError(f"Actual sample has no bilingual title: {identity}")
    catalog = (DIRECTORY / "generated_ant_design_reference_catalog.inc").read_text(encoding="utf-8")
    entries = set(re.findall(r'AntDesignReferenceEntry\{"([^"]+)"', catalog))
    for identity in entries:
        for field in ("name", "summary", "supported", "missing"):
            if "gallery." + identity + "." + field not in declarations:
                raise ValueError(f"Current source catalog entry is not covered: {identity}.{field}")
    fixtures = json.loads((root / "tests/fixtures/gallery-i18n-literals.json").read_text(encoding="utf-8"))
    registered = set()
    for fixture in fixtures:
        path = root / fixture["source"]
        source = path.read_text(encoding="utf-8")
        if not fixture["reason"] or not any(
            value == fixture["literal"] and containing_function(source, start) == fixture.get("function")
            for value, start, _ in source_literals(source)
        ):
            raise ValueError(f"Registered literal/source changed: {fixture['source']}")
        if "function" not in fixture or fixture.get("category") not in {"business-payload", "text-fixture", "code-or-api"}:
            raise ValueError(f"Fixture needs a precise function and category: {fixture['source']}")
        registered.add((fixture["source"], fixture["function"], fixture["literal"]))
    failures = []
    for name in ("token_gallery_definition.cpp", "gallery_pages_host.cpp", "gallery_page_controller.cpp", "gallery_source_panel.cpp"):
        relative = "examples/token_gallery/" + name
        failures.extend(unlocalized_ui(relative, (root / relative).read_text(encoding="utf-8"), registered))
    if failures:
        raise ValueError("Unlocalized Gallery display paths:\n" + "\n".join(failures))
    print(f"Gallery authored messages: {len(declarations)}; actual sample titles: {len(samples)}; "
          f"source catalog entries: {len(entries)}; registered literal sources: {len(fixtures)}")
    print("Authored display sinks and exact fixture provenance checked; runtime/platform success requires parent execution.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    check(parser.parse_args().root.resolve())
