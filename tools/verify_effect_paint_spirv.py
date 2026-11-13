#!/usr/bin/env python3
"""Check actual SPIR-V types/decorations absent from shadercross JSON.

This is SPIR-V reflection only. DXIL generation or container magic does not
constitute DXIL reflection or real GPU consumption.
"""

import argparse
from pathlib import Path
import struct
import shutil
import tempfile


class Module:
    def __init__(self, path: Path):
        data = path.read_bytes()
        if len(data) < 20 or len(data) % 4:
            raise ValueError(f"Invalid SPIR-V byte length: {path}")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        if words[0] != 0x07230203:
            raise ValueError(f"Invalid SPIR-V magic: {path}")
        self.types = {}
        self.variables = {}
        self.decorations = {}
        self.members = {}
        self.path = path
        offset = 5
        while offset < len(words):
            count = words[offset] >> 16
            opcode = words[offset] & 0xffff
            if count == 0 or offset + count > len(words):
                raise ValueError(f"Invalid SPIR-V instruction at word {offset}: {path}")
            args = words[offset + 1:offset + count]
            if opcode in (21, 22, 23, 28, 29, 30, 32):
                self.types[args[0]] = (opcode, args[1:])
            elif opcode == 59:
                self.variables[args[1]] = (args[0], args[2])
            elif opcode == 71:
                self.decorations.setdefault(args[0], {})[args[1]] = args[2:]
            elif opcode == 72:
                self.members.setdefault((args[0], args[1]), {})[args[2]] = args[3:]
            offset += count

    def require(self, condition, description):
        if not condition:
            raise ValueError(f"{self.path.name}: {description}")

    def pointed(self, variable):
        pointer, storage = self.variables[variable]
        opcode, args = self.types[pointer]
        self.require(opcode == 32 and args[0] == storage, "variable pointer storage class")
        return args[1]

    def vector(self, type_id, unsigned=False):
        opcode, args = self.types[type_id]
        if opcode != 23 or args[1] != 4:
            return False
        scalar, fields = self.types[args[0]]
        return (scalar == 21 and fields == (32, 0)) if unsigned else (scalar == 22 and fields == (32,))

    def locations(self, storage):
        result = {}
        for variable, (_, kind) in self.variables.items():
            decoration = self.decorations.get(variable, {})
            if kind == storage and 30 in decoration:
                location = decoration[30][0]
                self.require(location not in result, "duplicate interface location")
                result[location] = variable
        return result

    def resources(self):
        return {variable: self.decorations.get(variable, {}) for variable, (_, kind) in self.variables.items()
                if kind in (0, 2, 12)}

    def interface(self, storage, count, integer_locations=()):
        locations = self.locations(storage)
        self.require(set(locations) == set(range(count)), "input attribute locations/count")
        for location, variable in locations.items():
            self.require(self.vector(self.pointed(variable), location in integer_locations),
                         f"attribute {location} must be " + ("uint4" if location in integer_locations else "float4"))

    def flat_link(self, storage, location=6):
        variable = self.locations(storage).get(location)
        self.require(variable is not None, "missing link varying at location 6")
        self.require(self.vector(self.pointed(variable), True), "link varying must be uint4")
        self.require(14 in self.decorations.get(variable, {}), "link varying must have Flat decoration")

    def storage_layout(self):
        resources = self.resources()
        self.require(len(resources) == 2, "fragment must have exactly two storage resources")
        bindings = {}
        for variable, decorations in resources.items():
            self.require(decorations.get(34) == (2,), "fragment storage descriptor set must be 2")
            binding = decorations.get(33, (-1,))[0]
            self.require(binding in (0, 1) and binding not in bindings, "storage binding must be unique 0/1")
            bindings[binding] = variable
            outer = self.pointed(variable)
            opcode, members = self.types[outer]
            self.require(opcode == 30 and len(members) == 1, "storage wrapper must have one runtime array")
            self.require(self.members.get((outer, 0), {}).get(35) == (0,), "storage array offset must be zero")
            array = members[0]
            opcode, element = self.types[array]
            self.require(opcode == 29, "storage payload must be a runtime array")
            stride = 48 if binding == 0 else 32
            self.require(self.decorations.get(array, {}).get(6) == (stride,),
                         f"storage binding {binding} ArrayStride must be {stride}")
            element_id = element[0]
            opcode, fields = self.types[element_id]
            count = 3 if binding == 0 else 2
            self.require(opcode == 30 and len(fields) == count,
                         f"storage binding {binding} must contain exactly {count} vectors")
            for index, field in enumerate(fields):
                self.require(self.members.get((element_id, index), {}).get(35) == (index * 16,),
                             f"storage member {index} offset must be {index * 16}")
                self.require(self.vector(field, binding == 0),
                             f"storage binding {binding} member {index} type")
        self.require(set(bindings) == {0, 1}, "field/stop storage bindings")


def validate(directory):
    for name in ("rounded_effect", "rounded_effect_paint"):
        vertex = Module(directory / (name + ".vertex.spv"))
        fragment = Module(directory / (name + ".fragment.spv"))
        paint = name != "rounded_effect"
        vertex.interface(1, 8 if paint else 7, (7,) if paint else ())
        vertex.require(not vertex.resources(), "vertex resources must be zero")
        if paint:
            vertex.flat_link(3)
            fragment.flat_link(1)
            fragment.storage_layout()
            for opcode, fields in fragment.types.values():
                fragment.require(not (opcode in (21, 22) and fields[0] == 64),
                                 "paint arithmetic must not require optional 64-bit scalar types")
        else:
            fragment.require(not fragment.resources(), "legacy fragment resources must be zero")

def negative_test(directory, output):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="spirv-layout-", dir=output) as temporary:
        root = Path(temporary)
        for name in ("rounded_effect", "rounded_effect_paint"):
            for stage in ("vertex", "fragment"):
                filename = name + "." + stage + ".spv"
                shutil.copyfile(directory / filename, root / filename)
        for decoration, replacement in ((6, 64), (34, 3), (33, 7), (14, None)):
            path = root / "rounded_effect_paint.fragment.spv"
            original = (directory / path.name).read_bytes()
            link_variable = Module(directory / path.name).locations(1)[6]
            words = list(struct.unpack("<" + "I" * (len(original) // 4), original))
            offset = 5
            changed = False
            while offset < len(words):
                count = words[offset] >> 16
                opcode = words[offset] & 0xffff
                if (opcode == 71 and words[offset + 2] == decoration and
                        (decoration != 14 or words[offset + 1] == link_variable)):
                    if replacement is None:
                        words[offset + 2] = 0  # Replace Flat by RelaxedPrecision.
                    else:
                        words[offset + 3] = replacement
                    changed = True
                    break
                offset += count
            if not changed:
                raise ValueError(f"Negative fixture has no decoration {decoration}")
            path.write_bytes(struct.pack("<" + "I" * len(words), *words))
            try:
                validate(root)
            except ValueError:
                pass
            else:
                raise ValueError(f"Corrupted SPIR-V decoration {decoration} passed layout validation")
            path.write_bytes(original)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--negative-test", type=Path)
    args = parser.parse_args()
    validate(args.directory)
    if args.negative_test:
        negative_test(args.directory, args.negative_test)
    print("SPIR-V reflection: legacy=7xf32x4/0, paint=7xf32x4+u32x4/2; Flat, set2[0,1], "
          "field48=3xu32x4@[0,16,32], stop32=2xf32x4@[0,16]; cap excluded")


if __name__ == "__main__":
    main()
