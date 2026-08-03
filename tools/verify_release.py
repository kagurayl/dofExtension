#!/usr/bin/env python
"""Verify the release DLL and source-side registration contracts."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import pefile

EXPECTED_EXPORTS = {"StartHook": 1, "StopHook": 2}
EXPECTED_CLOSURES = 1803
EXPECTED_CALLBACKS = 32
RESOURCE_TYPE_RCDATA = 10
RESOURCE_ID_CLOSURES = 101


def resource_payload(pe: pefile.PE, resource_type: int, resource_id: int) -> bytes | None:
    root = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
    if root is None:
        return None
    for type_entry in root.entries:
        if type_entry.id != resource_type or not hasattr(type_entry, "directory"):
            continue
        for id_entry in type_entry.directory.entries:
            if id_entry.id != resource_id or not hasattr(id_entry, "directory"):
                continue
            language = id_entry.directory.entries[0]
            data = language.data.struct
            return pe.get_memory_mapped_image()[data.OffsetToData : data.OffsetToData + data.Size]
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    dll = args.dll.resolve()
    dll_bytes = dll.read_bytes()
    pe = pefile.PE(str(dll), fast_load=False)

    exports = {
        symbol.name.decode("ascii"): symbol.ordinal
        for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols
        if symbol.name
    }
    imports = sorted(
        entry.dll.decode("ascii") for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])
    )
    payload = resource_payload(pe, RESOURCE_TYPE_RCDATA, RESOURCE_ID_CLOSURES)

    callback_source = (root / "src/core/callback_registry.cpp").read_text(encoding="utf-8")
    callback_declared = int(re.search(r"array<Binding,\s*(\d+)>", callback_source).group(1))
    closure_files = len(list((root / "assets/closures").glob("*.cnut")))
    dispatch_entries = sum(
        1
        for line in (root / "generated/current_chr_dispatch.inc").read_text(encoding="utf-8").splitlines()
        if line.lstrip().startswith("{")
    )

    generated_payload = (root / "generated/all_closures.bin").read_bytes()
    checks = {
        "machine_is_x86": pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE["IMAGE_FILE_MACHINE_I386"],
        "exports_exact": exports == EXPECTED_EXPORTS,
        "imports_kernel32_only": [name.upper() for name in imports] == ["KERNEL32.DLL"],
        "closure_resource_present": payload is not None,
        "closure_resource_magic": payload is not None and payload.startswith(b"CQBC"),
        "closure_resource_magic_unique": payload is not None and payload.count(b"CQBC") == 1,
        "closure_resource_matches_generated": payload == generated_payload,
        "callback_count": callback_declared == EXPECTED_CALLBACKS,
        "closure_file_count": closure_files == EXPECTED_CLOSURES,
        "dispatch_entry_count": dispatch_entries == EXPECTED_CLOSURES,
    }
    result = {
        "ok": all(checks.values()),
        "dll": str(dll),
        "size": len(dll_bytes),
        "sha256": hashlib.sha256(dll_bytes).hexdigest(),
        "machine": f"0x{pe.FILE_HEADER.Machine:04X}",
        "exports": exports,
        "imports": imports,
        "closure_resource_bytes": len(payload) if payload is not None else 0,
        "callback_count": callback_declared,
        "closure_file_count": closure_files,
        "dispatch_entry_count": dispatch_entries,
        "checks": checks,
    }
    text = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True)
    print(text)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(text + "\n", encoding="utf-8")
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
