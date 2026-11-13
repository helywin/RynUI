"""CPU-only PE import provenance; no browser process, PATH search or dependency installation.

All bundled DLLs and their eager import closure are checked. Delay-loaded DLLs and
runtime-selected GPU/driver modules require the parent's actual browser execution.
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import hashlib
from pathlib import Path
import struct


def imports(path):
    data = Path(path).read_bytes()
    def number(fmt, offset):
        return struct.unpack_from(fmt, data, offset)[0]
    try:
        if data[:2] != b'MZ':
            raise ValueError('not a PE executable: ' + str(path))
        pe = number('<I', 0x3c)
        if data[pe:pe + 4] != b'PE\0\0' or number('<H', pe + 4) != 0x8664:
            raise ValueError('expected Windows x64 PE: ' + str(path))
        optional = pe + 24
        if number('<H', optional) != 0x20b:
            raise ValueError('expected PE32+: ' + str(path))
        sections = optional + number('<H', pe + 20)
        def offset(rva):
            for section in range(number('<H', pe + 6)):
                entry = sections + section * 40
                size = max(number('<I', entry + 8), number('<I', entry + 16))
                base = number('<I', entry + 12)
                if base <= rva < base + size:
                    return number('<I', entry + 20) + rva - base
            raise ValueError('unmapped PE import RVA')
        rva = number('<I', optional + 120)  # PE32+ data directory 1: eager imports.
        if not rva:
            return []
        entry = offset(rva)
        result = []
        while any(data[entry:entry + 20]):
            name = offset(number('<I', entry + 12))
            end = data.index(b'\0', name)
            result.append(data[name:end].decode('ascii').lower())
            entry += 20
        return sorted(set(result))
    except (IndexError, struct.error, UnicodeError) as error:
        raise ValueError('invalid PE import table: ' + str(path)) from error


def fingerprints(executable):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetSystemDirectoryW.argtypes = [wintypes.LPWSTR, wintypes.UINT]
    kernel.GetSystemDirectoryW.restype = wintypes.UINT
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.GetModuleFileNameW.argtypes = [wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
    kernel.GetModuleFileNameW.restype = wintypes.DWORD
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    kernel.FreeLibrary.restype = wintypes.BOOL
    buffer = ctypes.create_unicode_buffer(32768)
    if not kernel.GetSystemDirectoryW(buffer, len(buffer)):
        raise ctypes.WinError(ctypes.get_last_error())
    system = Path(buffer.value).resolve()
    executable = Path(executable).resolve()
    bundled = {p.name.lower(): p for p in executable.parent.glob('*.dll')}
    pending = [executable, *bundled.values()]
    files = {}
    contracts = {}
    while pending:
        path = pending.pop().resolve()
        if str(path) in files:
            continue
        files[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        for name in imports(path):
            if Path(name).name != name:
                raise ValueError('invalid import name: ' + name)
            if name in bundled:
                dependency = bundled[name]
            elif name.startswith(('api-ms-', 'ext-ms-')):
                # Resolve OS API contracts through the System32-only loader;
                # never treat a missing api-ms DLL file as a missing contract.
                handle = kernel.LoadLibraryExW(name, None, 0x800)
                if not handle:
                    raise OSError('unresolved Windows API contract: ' + name)
                try:
                    if not kernel.GetModuleFileNameW(handle, buffer, len(buffer)):
                        raise ctypes.WinError(ctypes.get_last_error())
                    dependency = Path(buffer.value).resolve()
                    if not dependency.is_relative_to(system):
                        raise ValueError('API contract outside System32: ' + name)
                    contracts[name] = str(dependency)
                finally:
                    kernel.FreeLibrary(handle)
            else:
                dependency = system / name
            if not dependency.is_file():
                raise OSError('missing Windows runtime dependency: ' + str(dependency))
            pending.append(dependency)
    return {'method': 'pe-eager-imports-and-bundled-dlls-v1', 'files': dict(sorted(files.items())),
            'api_sets': dict(sorted(contracts.items()))}
